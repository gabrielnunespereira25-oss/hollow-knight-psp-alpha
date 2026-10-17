#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspge.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "png_loader.h"

PSP_MODULE_INFO("Dark Bot", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define MAX_FRAMES 10

static unsigned int __attribute__((aligned(16)))
display_list[262144];

typedef struct {
    unsigned short u, v;
    short x, y, z;
} SpriteVertex;

typedef struct {
    int width;
    int height;
    int tex_width;
    int tex_height;
    unsigned int *pixels;
} Sprite;

typedef struct {
    Sprite frames[MAX_FRAMES];
    int count;
} Animation;

static Animation idle_anim;
static Animation walk_anim;
static Animation run_anim;
static Animation jump_anim;

static int running = 1;
static int in_game = 0;
static int menu_option = 0;

static float player_x = 80.0f;
static float player_y = 170.0f;
static float player_vy = 0.0f;
static int on_ground = 1;
static int facing_right = 1;

static const char *menu_items[] = {
    "NOVO JOGO",
    "CONTINUAR",
    "OPCOES",
    "CREDITOS",
    "SAIR"
};

static int next_power_of_two(int value)
{
    int result = 1;

    while (result < value)
        result <<= 1;

    return result;
}

static void free_sprite(Sprite *sprite)
{
    if (sprite->pixels != NULL)
        free(sprite->pixels);

    memset(sprite, 0, sizeof(*sprite));
}

static int load_sprite(const char *path, Sprite *sprite)
{
    PngImage image;
    int x, y;

    memset(&image, 0, sizeof(image));
    memset(sprite, 0, sizeof(*sprite));

    if (psp_png_load(path, &image) != 0)
        return 0;

    if (image.width <= 0 || image.height <= 0 ||
        image.width > 512 || image.height > 512 ||
        image.pixels == NULL) {
        psp_png_free(&image);
        return 0;
    }

    sprite->width = image.width;
    sprite->height = image.height;
    sprite->tex_width = next_power_of_two(image.width);
    sprite->tex_height = next_power_of_two(image.height);

    sprite->pixels = memalign(
        16,
        sprite->tex_width * sprite->tex_height * sizeof(unsigned int)
    );

    if (sprite->pixels == NULL) {
        psp_png_free(&image);
        return 0;
    }

    memset(
        sprite->pixels,
        0,
        sprite->tex_width * sprite->tex_height * sizeof(unsigned int)
    );

    for (y = 0; y < image.height; y++) {
        for (x = 0; x < image.width; x++) {
            sprite->pixels[
                y * sprite->tex_width + x
            ] = image.pixels[y * image.width + x];
        }
    }

    psp_png_free(&image);
    return 1;
}

static void free_animation(Animation *animation)
{
    int i;

    for (i = 0; i < animation->count; i++)
        free_sprite(&animation->frames[i]);

    animation->count = 0;
}

static void load_animation(Animation *animation, const char *folder)
{
    int i;
    char path[128];

    animation->count = 0;

    for (i = 0; i < MAX_FRAMES; i++) {
        snprintf(
            path,
            sizeof(path),
            "assets/%s/%s_%02d.png",
            folder,
            folder,
            i
        );

        if (!load_sprite(path, &animation->frames[i]))
            break;

        animation->count++;
    }

    /*
     * Caso o arquivo use idle_00.png, walk_00.png,
     * run_00.png ou jump_00.png, o carregamento comeca
     * pelo quadro 00 de cada pasta.
     */
}

static void load_all_animations(void)
{
    load_animation(&idle_anim, "idle");
    load_animation(&walk_anim, "walk");
    load_animation(&run_anim, "run");
    load_animation(&jump_anim, "jump");
}

static void free_all_animations(void)
{
    free_animation(&idle_anim);
    free_animation(&walk_anim);
    free_animation(&run_anim);
    free_animation(&jump_anim);
}

static void setup_game_graphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, display_list);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        512
    );

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void *)0x88000,
        512
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        512
    );

    sceGuOffset(
        2048 - (SCREEN_W / 2),
        2048 - (SCREEN_H / 2)
    );

    sceGuViewport(
        2048,
        2048,
        SCREEN_W,
        SCREEN_H
    );

    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);

    sceGuBlendFunc(
        GU_ADD,
        GU_SRC_ALPHA,
        GU_ONE_MINUS_SRC_ALPHA,
        0,
        0
    );

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    sceGuClearColor(0xFF202838);
    sceGuClearDepth(0);
    sceGuClear(
        GU_COLOR_BUFFER_BIT |
        GU_DEPTH_BUFFER_BIT
    );

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void draw_sprite(Sprite *sprite, float x, float y)
{
    SpriteVertex *vertices;

    if (sprite == NULL || sprite->pixels == NULL)
        return;

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);

    sceGuTexImage(
        0,
        sprite->tex_width,
        sprite->tex_height,
        sprite->tex_width,
        sprite->pixels
    );

    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);

    vertices = (SpriteVertex *)sceGuGetMemory(
        2 * sizeof(SpriteVertex)
    );

    if (vertices == NULL)
        return;

    vertices[0].u = 0;
    vertices[0].v = 0;
    vertices[0].x = (short)x;
    vertices[0].y = (short)y;
    vertices[0].z = 0;

    vertices[1].u = (unsigned short)sprite->width;
    vertices[1].v = (unsigned short)sprite->height;
    vertices[1].x = (short)(x + sprite->width);
    vertices[1].y = (short)(y + sprite->height);
    vertices[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_16BIT |
        GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2,
        NULL,
        vertices
    );
}

static void draw_game(Animation *animation, int frame)
{
    Sprite *sprite = NULL;

    sceGuStart(GU_DIRECT, display_list);

    sceGuClearColor(0xFF202838);
    sceGuClear(
        GU_COLOR_BUFFER_BIT |
        GU_DEPTH_BUFFER_BIT
    );

    /*
     * Chao de teste.
     */
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);

    sceGuFinish();
    sceGuSync(0, 0);

    sceGuStart(GU_DIRECT, display_list);

    sceGuClearColor(0xFF202838);

    if (animation != NULL && animation->count > 0) {
        frame %= animation->count;
        sprite = &animation->frames[frame];
    }

    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);

    sceGuBlendFunc(
        GU_ADD,
        GU_SRC_ALPHA,
        GU_ONE_MINUS_SRC_ALPHA,
        0,
        0
    );

    if (sprite != NULL) {
        float draw_x = player_x;
        float draw_y = player_y;

        /*
         * A direcao e guardada para permitir
         * adicionar o espelhamento do sprite depois.
         */
        if (!facing_right)
            draw_x = player_x;

        draw_sprite(sprite, draw_x, draw_y);
    } else {
        /*
         * Reserva um marcador visivel se ainda
         * nao houver sprites PNG carregados.
         */
        sceGuDisable(GU_TEXTURE_2D);
        sceGuDisable(GU_BLEND);
    }

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static Animation *get_current_animation(int moving, int sprinting)
{
    if (!on_ground && jump_anim.count > 0)
        return &jump_anim;

    if (moving && sprinting && run_anim.count > 0)
        return &run_anim;

    if (moving && walk_anim.count > 0)
        return &walk_anim;

    return &idle_anim;
}

static void start_new_game(void)
{
    player_x = 80.0f;
    player_y = 170.0f;
    player_vy = 0.0f;
    on_ground = 1;
    facing_right = 1;
    in_game = 1;
}

static void draw_menu(void)
{
    int i;

    pspDebugScreenInit();
    pspDebugScreenSetTextColor(0xFFFFFFFF);

    pspDebugScreenSetXY(14, 3);
    pspDebugScreenPrintf("DARK BOT");

    pspDebugScreenSetXY(10, 5);
    pspDebugScreenPrintf("PSP - BETA");

    for (i = 0; i < 5; i++) {
        pspDebugScreenSetXY(12, 9 + i * 2);

        if (i == menu_option)
            pspDebugScreenPrintf("> %s", menu_items[i]);
        else
            pspDebugScreenPrintf("  %s", menu_items[i]);
    }

    pspDebugScreenSetXY(5, 23);
    pspDebugScreenPrintf("D-PAD: MOVER   X: SELECIONAR");
}

int main(void)
{
    SceCtrlData pad;
    SceCtrlData old_pad;

    int frame = 0;
    int moving = 0;
    int sprinting = 0;

    memset(&old_pad, 0, sizeof(old_pad));

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    pspDebugScreenInit();

    load_all_animations();

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (!in_game) {
            draw_menu();

            if ((pad.Buttons & PSP_CTRL_UP) &&
                !(old_pad.Buttons & PSP_CTRL_UP)) {
                menu_option--;

                if (menu_option < 0)
                    menu_option = 4;
            }

            if ((pad.Buttons & PSP_CTRL_DOWN) &&
                !(old_pad.Buttons & PSP_CTRL_DOWN)) {
                menu_option++;

                if (menu_option > 4)
                    menu_option = 0;
            }

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(old_pad.Buttons & PSP_CTRL_CROSS)) {
                switch (menu_option) {
                    case 0:
                    case 1:
                        setup_game_graphics();
                        start_new_game();
                        break;

                    case 2:
                        pspDebugScreenClear();
                        pspDebugScreenSetXY(5, 5);
                        pspDebugScreenPrintf("OPCOES");
                        pspDebugScreenSetXY(5, 7);
                        pspDebugScreenPrintf("Use o direcional para mover.");
                        pspDebugScreenSetXY(5, 9);
                        pspDebugScreenPrintf("R: correr | X: pular");
                        pspDebugScreenSetXY(5, 11);
                        pspDebugScreenPrintf("O: voltar");
                        sceKernelDelayThread(1800000);
                        break;

                    case 3:
                        pspDebugScreenClear();
                        pspDebugScreenSetXY(5, 5);
                        pspDebugScreenPrintf("DARK BOT");
                        pspDebugScreenSetXY(5, 7);
                        pspDebugScreenPrintf("Projeto beta para PSP.");
                        pspDebugScreenSetXY(5, 9);
                        pspDebugScreenPrintf("Sprites PNG personalizados.");
                        pspDebugScreenSetXY(5, 11);
                        pspDebugScreenPrintf("O: voltar");
                        sceKernelDelayThread(1800000);
                        break;

                    case 4:
                        running = 0;
                        break;
                }
            }
        } else {
            float speed = 2.0f;

            moving = 0;
            sprinting = 0;

            if (pad.Buttons & PSP_CTRL_RTRIGGER) {
                speed = 3.5f;
                sprinting = 1;
            }

            if (pad.Buttons & PSP_CTRL_LEFT) {
                player_x -= speed;
                facing_right = 0;
                moving = 1;
            }

            if (pad.Buttons & PSP_CTRL_RIGHT) {
                player_x += speed;
                facing_right = 1;
                moving = 1;
            }

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(old_pad.Buttons & PSP_CTRL_CROSS) &&
                on_ground) {
                player_vy = -8.0f;
                on_ground = 0;
            }

            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                in_game = 0;
                pspDebugScreenInit();
                continue;
            }

            player_vy += 0.35f;
            player_y += player_vy;

            if (player_y >= 170.0f) {
                player_y = 170.0f;
                player_vy = 0.0f;
                on_ground = 1;
            }

            if (player_x < 0.0f)
                player_x = 0.0f;

            if (player_x > SCREEN_W - 32)
                player_x = SCREEN_W - 32;

            {
                Animation *current =
                    get_current_animation(moving, sprinting);

                draw_game(current, frame / 8);
            }

            frame++;
        }

        old_pad = pad;
        sceDisplayWaitVblankStart();
    }

    free_all_animations();

    sceKernelExitGame();
    return 0;
}
