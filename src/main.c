#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <psputility.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "png_loader.h"

PSP_MODULE_INFO("Dark Bot", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define MAX_FRAMES 10

static unsigned int __attribute__((aligned(16))) display_list[262144];

typedef struct {
    unsigned short u, v;
    short x, y, z;
} Vertex;

typedef struct {
    int width, height;
    int tex_width, tex_height;
    unsigned int *pixels;
} Sprite;

typedef struct {
    Sprite frames[MAX_FRAMES];
    int count;
} Animation;

static Animation idle_anim, walk_anim, run_anim, jump_anim;
static int gu_ready = 0;

static int player_x = 220;
static int player_y = 150;
static int velocity_y = 0;
static int jumping = 0;
static int facing_left = 0;
static int frame_index = 0;
static int frame_timer = 0;

static int next_power_of_two(int n)
{
    int p = 1;
    while (p < n && p < 1024) p <<= 1;
    return p;
}

static void sprite_free(Sprite *s)
{
    if (s->pixels) {
        free(s->pixels);
        s->pixels = NULL;
    }
}

static int sprite_load(const char *path, Sprite *s)
{
    PngImage img;
    memset(s, 0, sizeof(*s));
    memset(&img, 0, sizeof(img));

    if (psp_png_load(path, &img) < 0)
        return 0;

    if (!img.pixels || img.width <= 0 || img.height <= 0) {
        psp_png_free(&img);
        return 0;
    }

    s->width = img.width;
    s->height = img.height;
    s->tex_width = next_power_of_two(img.width);
    s->tex_height = next_power_of_two(img.height);

    s->pixels = memalign(16,
        s->tex_width * s->tex_height * sizeof(unsigned int));

    if (!s->pixels) {
        psp_png_free(&img);
        return 0;
    }

    memset(s->pixels, 0,
        s->tex_width * s->tex_height * sizeof(unsigned int));

    for (int y = 0; y < img.height; y++) {
        memcpy(s->pixels + y * s->tex_width,
               img.pixels + y * img.width,
               img.width * sizeof(unsigned int));
    }

    psp_png_free(&img);
    return 1;
}

static void animation_load(Animation *a, const char *folder,
                           const char *prefix)
{
    char path[128];
    memset(a, 0, sizeof(*a));

    for (int i = 0; i < MAX_FRAMES; i++) {
        snprintf(path, sizeof(path),
                 "assets/%s/%s_%02d.png", folder, prefix, i);

        if (!sprite_load(path, &a->frames[i]))
            break;

        a->count++;
    }

    /* Tenta também a imagem sem número. */
    if (a->count == 0) {
        snprintf(path, sizeof(path), "assets/%s.png", prefix);
        if (sprite_load(path, &a->frames[0]))
            a->count = 1;
    }
}

static void animation_free(Animation *a)
{
    for (int i = 0; i < a->count; i++)
        sprite_free(&a->frames[i]);
    a->count = 0;
}

static void load_assets(void)
{
    animation_load(&idle_anim, "idle", "idle");
    animation_load(&walk_anim, "walk", "walk");
    animation_load(&run_anim, "run", "run");
    animation_load(&jump_anim, "jump", "jump");

    /* Usa idle como alternativa se alguma animação estiver ausente. */
    if (walk_anim.count == 0 && idle_anim.count > 0)
        walk_anim = idle_anim;
    if (run_anim.count == 0 && idle_anim.count > 0)
        run_anim = idle_anim;
    if (jump_anim.count == 0 && idle_anim.count > 0)
        jump_anim = idle_anim;
}

static void graphics_init(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, display_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)0x88000, 512);
    sceGuDepthBuffer((void *)0x110000, 512);

    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA,
                   GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    gu_ready = 1;
}

static void sprite_draw(Sprite *s, int x, int y)
{
    if (!s || !s->pixels) return;

    sceKernelDcacheWritebackRange(
        s->pixels,
        s->tex_width * s->tex_height * sizeof(unsigned int));

    sceGuTexImage(0, s->tex_width, s->tex_height,
                  s->tex_width, s->pixels);

    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    if (!v) return;

    int draw_w = s->width;
    int draw_h = s->height;

    if (!facing_left) {
        v[0].u = 0;
        v[0].v = 0;
        v[1].u = s->width;
        v[1].v = s->height;
    } else {
        v[0].u = s->width;
        v[0].v = 0;
        v[1].u = 0;
        v[1].v = s->height;
    }

    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].x = x + draw_w;
    v[1].y = y + draw_h;
    v[1].z = 0;

    sceGuDrawArray(GU_SPRITES,
        GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        2, NULL, v);
}

static Animation *current_animation(int moving, int sprinting)
{
    if (jumping && jump_anim.count > 0)
        return &jump_anim;
    if (moving && sprinting && run_anim.count > 0)
        return &run_anim;
    if (moving && walk_anim.count > 0)
        return &walk_anim;
    return &idle_anim;
}

static void draw_frame(void)
{
    sceGuStart(GU_DIRECT, display_list);

    /* Fundo sólido: nenhum cenário, mapa ou imagem de fundo. */
    sceGuClearColor(0xFF202838);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);

    Animation *anim = current_animation(0, 0);

    if (anim && anim->count > 0) {
        int index = frame_index % anim->count;
        sprite_draw(&anim->frames[index], player_x, player_y);
    }

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void update_game(void)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    int moving = 0;
    int sprinting = (pad.Buttons & PSP_CTRL_RTRIGGER) != 0;
    int speed = sprinting ? 4 : 2;

    if (pad.Buttons & PSP_CTRL_LEFT) {
        player_x -= speed;
        facing_left = 1;
        moving = 1;
    }

    if (pad.Buttons & PSP_CTRL_RIGHT) {
        player_x += speed;
        facing_left = 0;
        moving = 1;
    }

    if ((pad.Buttons & PSP_CTRL_CROSS) && !jumping) {
        jumping = 1;
        velocity_y = -9;
    }

    if (jumping) {
        player_y += velocity_y;
        velocity_y++;

        if (player_y >= 150) {
            player_y = 150;
            velocity_y = 0;
            jumping = 0;
        }
    }

    if (player_x < 0) player_x = 0;
    if (player_x > SCREEN_W - 32) player_x = SCREEN_W - 32;

    Animation *anim = current_animation(moving, sprinting);

    if (anim && anim->count > 1) {
        frame_timer++;
        if (frame_timer >= (sprinting ? 4 : 7)) {
            frame_timer = 0;
            frame_index++;
        }
    } else {
        frame_index = 0;
        frame_timer = 0;
    }
}

static void free_assets(void)
{
    animation_free(&idle_anim);

    /* Evita liberar duas vezes se as animações forem cópias de idle. */
    if (walk_anim.frames[0].pixels != idle_anim.frames[0].pixels)
        animation_free(&walk_anim);
    if (run_anim.frames[0].pixels != idle_anim.frames[0].pixels)
        animation_free(&run_anim);
    if (jump_anim.frames[0].pixels != idle_anim.frames[0].pixels)
        animation_free(&jump_anim);
}

int main(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    pspDebugScreenInit();
    pspDebugScreenSetTextColor(0xFFFFFFFF);

    load_assets();

    int running = 1;

    while (running) {
        pspDebugScreenClear();
        pspDebugScreenSetXY(12, 4);
        pspDebugScreenPrintf("DARK BOT");
        pspDebugScreenSetXY(12, 7);
        pspDebugScreenPrintf("X - NOVO JOGO");
        pspDebugScreenSetXY(12, 9);
        pspDebugScreenPrintf("O - SAIR");

        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_CROSS) {
            player_x = 220;
            player_y = 150;
            jumping = 0;
            velocity_y = 0;
            frame_index = 0;

            graphics_init();

            int game = 1;
            while (game) {
                sceCtrlReadBufferPositive(&pad, 1);

                if (pad.Buttons & PSP_CTRL_CIRCLE) {
                    game = 0;
                    break;
                }

                update_game();
                draw_frame();
            }

            sceGuDisplay(GU_FALSE);
            sceGuTerm();
            gu_ready = 0;

            pspDebugScreenInit();
            pspDebugScreenSetTextColor(0xFFFFFFFF);
        }

        if (pad.Buttons & PSP_CTRL_CIRCLE)
            running = 0;

        sceDisplayWaitVblankStart();
    }

    free_assets();
    sceKernelExitGame();
    return 0;
}
