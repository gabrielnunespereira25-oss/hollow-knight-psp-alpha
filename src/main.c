#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspdebug.h>
#include <pspge.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "png_loader.h"

PSP_MODULE_INFO("Dark Bot", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define W 480
#define H 272
#define BUF_W 512
#define MAX_FRAMES 10

static unsigned int __attribute__((aligned(16)))
display_list[262144];

typedef struct {
    unsigned short u, v;
    short x, y, z;
} Vertex;

typedef struct {
    int width, height, tw, th;
    unsigned int *pixels;
} Sprite;

typedef struct {
    Sprite frames[MAX_FRAMES];
    int count;
} Animation;

static Animation idle, walk, run, jump;
static float px = 80.0f;
static float py = 170.0f;
static float vy = 0.0f;
static int grounded = 1;
static int playing = 0;
static int alive = 1;
static int menu = 0;
static int frame = 0;

static const char *items[] = {
    "NOVO JOGO",
    "CONTINUAR",
    "OPCOES",
    "CREDITOS",
    "SAIR"
};

static int pot(int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

static void sprite_free(Sprite *s)
{
    if (s->pixels) free(s->pixels);
    memset(s, 0, sizeof(*s));
}

static int sprite_load(const char *path, Sprite *s)
{
    PngImage img;
    int x, y;

    memset(s, 0, sizeof(*s));
    memset(&img, 0, sizeof(img));

    if (psp_png_load(path, &img) != 0)
        return 0;

    if (!img.pixels || img.width <= 0 || img.height <= 0 ||
        img.width > 512 || img.height > 512) {
        psp_png_free(&img);
        return 0;
    }

    s->width = img.width;
    s->height = img.height;
    s->tw = pot(img.width);
    s->th = pot(img.height);

    s->pixels = memalign(16,
        s->tw * s->th * sizeof(unsigned int));

    if (!s->pixels) {
        psp_png_free(&img);
        return 0;
    }

    memset(s->pixels, 0,
        s->tw * s->th * sizeof(unsigned int));

    for (y = 0; y < img.height; y++) {
        for (x = 0; x < img.width; x++) {
            s->pixels[y * s->tw + x] =
                img.pixels[y * img.width + x];
        }
    }

    psp_png_free(&img);
    return 1;
}

static void animation_load(Animation *a, const char *name)
{
    int i;
    char path[128];

    a->count = 0;

    for (i = 0; i < MAX_FRAMES; i++) {
        snprintf(path, sizeof(path),
            "assets/%s/%s_%02d.png", name, name, i);

        if (!sprite_load(path, &a->frames[i]))
            break;

        a->count++;
    }
}

static void animation_free(Animation *a)
{
    int i;
    for (i = 0; i < a->count; i++)
        sprite_free(&a->frames[i]);
    a->count = 0;
}

static void load_assets(void)
{
    animation_load(&idle, "idle");
    animation_load(&walk, "walk");
    animation_load(&run, "run");
    animation_load(&jump, "jump");
}

static void free_assets(void)
{
    animation_free(&idle);
    animation_free(&walk);
    animation_free(&run);
    animation_free(&jump);
}

static void graphics_init(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, display_list);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(W, H, (void *)0x88000, BUF_W);
    sceGuDepthBuffer((void *)0x110000, BUF_W);

    sceGuOffset(2048 - W / 2, 2048 - H / 2);
    sceGuViewport(2048, 2048, W, H);
    sceGuScissor(0, 0, W, H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);

    sceGuBlendFunc(GU_ADD,
        GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void sprite_draw(Sprite *s, float x, float y)
{
    Vertex *v;

    if (!s || !s->pixels) return;

    sceKernelDcacheWritebackRange(
        s->pixels,
        s->tw * s->th * sizeof(unsigned int));

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, s->tw, s->th, s->tw, s->pixels);

    v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    if (!v) return;

    v[0].u = 0;
    v[0].v = 0;
    v[0].x = (short)x;
    v[0].y = (short)y;
    v[0].z = 0;

    v[1].u = (unsigned short)s->width;
    v[1].v = (unsigned short)s->height;
    v[1].x = (short)(x + s->width);
    v[1].y = (short)(y + s->height);
    v[1].z = 0;

    sceGuDrawArray(GU_SPRITES,
        GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2, NULL, v);
}

static void draw_frame(Animation *a)
{
    Sprite *s = NULL;

    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(0xFF202838);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    if (a && a->count > 0)
        s = &a->frames[(frame / 8) % a->count];

    if (s && s->pixels) {
        sceGuEnable(GU_TEXTURE_2D);
        sceGuEnable(GU_BLEND);
        sprite_draw(s, px, py);
    } else {
        /*
         * Sem PNG, a tela permanece no fundo.
         * Os arquivos devem estar ao lado do EBOOT.PBP.
         */
    }

    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
    sceDisplayWaitVblankStart();
}

static void menu_draw(void)
{
    int i;

    pspDebugScreenInit();
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenSetXY(15, 3);
    pspDebugScreenPrintf("DARK BOT - PSP");

    for (i = 0; i < 5; i++) {
        pspDebugScreenSetXY(12, 8 + i * 2);
        pspDebugScreenPrintf("%s %s",
            i == menu ? ">" : " ", items[i]);
    }

    pspDebugScreenSetXY(4, 23);
    pspDebugScreenPrintf("D-PAD: MOVER   X: ESCOLHER");
}

static Animation *choose_animation(int moving, int sprinting)
{
    if (!grounded && jump.count) return &jump;
    if (moving && sprinting && run.count) return &run;
    if (moving && walk.count) return &walk;
    return &idle;
}

int main(void)
{
    SceCtrlData pad, old;
    int moving, sprinting;

    memset(&old, 0, sizeof(old));
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    pspDebugScreenInit();
    load_assets();

    while (alive) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (!playing) {
            menu_draw();

            if ((pad.Buttons & PSP_CTRL_UP) &&
                !(old.Buttons & PSP_CTRL_UP))
                menu = (menu + 4) % 5;

            if ((pad.Buttons & PSP_CTRL_DOWN) &&
                !(old.Buttons & PSP_CTRL_DOWN))
                menu = (menu + 1) % 5;

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(old.Buttons & PSP_CTRL_CROSS)) {
                if (menu == 0 || menu == 1) {
                    px = 80;
                    py = 170;
                    vy = 0;
                    grounded = 1;
                    graphics_init();
                    playing = 1;
                } else if (menu == 2 || menu == 3) {
                    pspDebugScreenClear();
                    pspDebugScreenSetXY(5, 5);
                    pspDebugScreenPrintf(
                        menu == 2 ? "OPCOES - R CORRER, X PULAR"
                                  : "DARK BOT - VERSAO BETA");
                    pspDebugScreenSetXY(5, 8);
                    pspDebugScreenPrintf("APERTE O PARA VOLTAR");
                } else {
                    alive = 0;
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
                px -= speed;
                moving = 1;
            }

            if (pad.Buttons & PSP_CTRL_RIGHT) {
                px += speed;
                moving = 1;
            }

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(old.Buttons & PSP_CTRL_CROSS) && grounded) {
                vy = -8.0f;
                grounded = 0;
            }

            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                playing = 0;
                pspDebugScreenInit();
                continue;
            }

            vy += 0.35f;
            py += vy;

            if (py >= 170.0f) {
                py = 170.0f;
                vy = 0;
                grounded = 1;
            }

            if (px < 0) px = 0;
            if (px > W - 32) px = W - 32;

            draw_frame(choose_animation(moving, sprinting));
            frame++;
        }

        old = pad;
        sceKernelDelayThread(1000);
    }

    free_assets();
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
