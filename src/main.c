#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <psputils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "png_loader.h"

PSP_MODULE_INFO("Dark Bot", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define MAX_FRAMES 10
#define WHITE 0xFFFFFFFF
#define GOLD  0xFFFFD080
#define GRAY  0xFFAAAAAA

typedef struct {
float x, y, vx, vy;
int grounded;
} Player;

typedef struct {
int width, height;
int tex_width, tex_height;
unsigned int *pixels;
} Sprite;

typedef struct {
Sprite frames[MAX_FRAMES];
int count;
} Animation;

typedef struct {
float u, v;
short x, y, z;
} SpriteVertex;

static unsigned int attribute((aligned(16)))
display_list[262144];

static Animation idle_anim, walk_anim, run_anim, jump_anim;

/* Próxima potência de dois, usada nas texturas do PSP. */
static int next_power_of_two(int n)
{
int p = 1;
while (p < n && p < 1024)
p <<= 1;
return p;
}

static void free_sprite(Sprite *s)
{
if (s->pixels)
free(s->pixels);

memset(s, 0, sizeof(*s));

}

static void free_animation(Animation *a)
{
int i;

for (i = 0; i < a->count; i++)
    free_sprite(&a->frames[i]);

a->count = 0;

}

static int load_sprite(const char *path, Sprite *s)
{
PngImage image;
int x, y;

memset(s, 0, sizeof(*s));

if (!psp_png_load(path, &image))
    return 0;

s->width = image.width;
s->height = image.height;
s->tex_width = next_power_of_two(image.width);
s->tex_height = next_power_of_two(image.height);

s->pixels = memalign(
    16,
    (size_t)s->tex_width * s->tex_height * 4
);

if (!s->pixels) {
    psp_png_free(&image);
    return 0;
}

memset(
    s->pixels, 0,
    (size_t)s->tex_width * s->tex_height * 4
);

for (y = 0; y < image.height; y++) {
    for (x = 0; x < image.width; x++) {
        s->pixels[y * s->tex_width + x] =
            image.pixels[y * image.width + x];
    }
}

psp_png_free(&image);
return 1;

}

static void load_animation(Animation *a, const char *folder,
const char *prefix)
{
int i;
char path[128];

a->count = 0;

for (i = 0; i < MAX_FRAMES; i++) {
    Sprite s;

    snprintf(path, sizeof(path),
             "assets/%s/%s_%02d.png",
             folder, prefix, i);

    if (!load_sprite(path, &s))
        break;

    a->frames[a->count++] = s;
}

}

static void setup_game_graphics(void)
{
sceGuInit();

sceGuStart(GU_DIRECT, display_list);

sceGuDrawBuffer(GU_PSM_8888,
                (void *)0, 512);
sceGuDispBuffer(SCREEN_W, SCREEN_H,
                (void *)0x88000, 512);
sceGuDepthBuffer((void *)0x110000, 512);

sceGuOffset(2048 - SCREEN_W / 2,
            2048 - SCREEN_H / 2);
sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
sceGuScissor(0, 0, SCREEN_W, SCREEN_H);

sceGuEnable(GU_SCISSOR_TEST);
sceGuDisable(GU_DEPTH_TEST);
sceGuDisable(GU_CULL_FACE);
sceGuEnable(GU_TEXTURE_2D);

sceGuTexMode(GU_PSM_8888, 0, 0, 0);
sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
sceGuTexFilter(GU_LINEAR, GU_LINEAR);
sceGuTexWrap(GU_CLAMP, GU_CLAMP);

sceGuFinish();
sceGuSync(0, 0);

sceDisplayWaitVblankStart();
sceGuDisplay(GU_TRUE);

}

static void draw_sprite(Sprite *s, int x, int y,
int draw_width, int draw_height)
{
SpriteVertex *v;

if (!s || !s->pixels)
    return;

sceGuTexImage(0, s->tex_width, s->tex_height,
              s->tex_width, s->pixels);

v = (SpriteVertex *)sceGuGetMemory(
    2 * sizeof(SpriteVertex)
);

if (!v)
    return;

v[0].u = 0.0f;
v[0].v = 0.0f;
v[0].x = x;
v[0].y = y;
v[0].z = 0;

v[1].u = (float)s->width;
v[1].v = (float)s->height;
v[1].x = x + draw_width;
v[1].y = y + draw_height;
v[1].z = 0;

sceGuDrawArray(
    GU_SPRITES,
    GU_TEXTURE_32BITF |
    GU_VERTEX_16BIT |
    GU_TRANSFORM_2D,
    2, 0, v
);

}

static void draw_game(Player *p, Sprite *sprite)
{
sceGuStart(GU_DIRECT, display_list);

sceGuClearColor(0xFF20202A);
sceGuClear(GU_COLOR_BUFFER_BIT);

/* Chão de teste. */
sceGuDisable(GU_TEXTURE_2D);
sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);

{
    typedef struct {
        unsigned int color;
        short x, y, z;
    } FloorVertex;

    FloorVertex *floor_v =
        (FloorVertex *)sceGuGetMemory(
            2 * sizeof(FloorVertex)
        );

    if (floor_v) {
        floor_v[0].color = 0xFF606070;
        floor_v[0].x = 0;
        floor_v[0].y = 240;
        floor_v[0].z = 0;

        floor_v[1].color = 0xFF606070;
        floor_v[1].x = SCREEN_W;
        floor_v[1].y = SCREEN_H;
        floor_v[1].z = 0;

        sceGuDrawArray(
            GU_SPRITES,
            GU_COLOR_8888 |
            GU_VERTEX_16BIT |
            GU_TRANSFORM_2D,
            2, 0, floor_v
        );
    }
}

sceGuEnable(GU_TEXTURE_2D);

if (sprite && sprite->pixels) {
    int w = sprite->width;
    int h = sprite->height;

    /* Evita sprites gigantes ocuparem a tela toda. */
    if (h > 112) {
        w = w * 112 / h;
        h = 112;
    }

    if (w > 160) {
        h = h * 160 / w;
        w = 160;
    }

    draw_sprite(sprite, (int)p->x,
                (int)p->y - h, w, h);
}

sceGuFinish();
sceGuSync(0, 0);
sceDisplayWaitVblankStart();
sceGuSwapBuffers();

}

static Sprite *choose_frame(Animation *a, int frame)
{
if (a->count <= 0)
return NULL;

return &a->frames[frame % a->count];

}

int main(void)
{
SceCtrlData pad;
Player player = {220.0f, 220.0f, 0.0f, 0.0f, 1};

const char *opcoes[] = {
    "NOVO JOGO", "CONTINUAR", "OPCOES",
    "CREDITOS", "SAIR"
};

int selecionado = 0;
int tela = 0;
int sair = 0;
int anterior = 0;
int frame = 0;
int anim_timer = 0;
int animacao = 0;
int direcao = 1;
unsigned int pressionado;

pspDebugScreenInit();

sceCtrlSetSamplingCycle(0);
sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

load_animation(&idle_anim, "idle", "idle");
load_animation(&walk_anim, "walk", "walk");
load_animation(&run_anim, "run", "run");
load_animation(&jump_anim, "jump", "jump");

while (!sair) {
    sceCtrlReadBufferPositive(&pad, 1);

    pressionado = pad.Buttons & ~anterior;
    anterior = pad.Buttons;

    if (tela == 0) {
        pspDebugScreenClear();

        pspDebugScreenSetTextColor(GRAY);
        pspDebugScreenSetXY(5, 2);
        pspDebugScreenPrintf("DARK BOT / PSP");

        pspDebugScreenSetTextColor(WHITE);
        pspDebugScreenSetXY(14, 5);
        pspDebugScreenPrintf("DARK BOT");

        pspDebugScreenSetTextColor(GOLD);
        pspDebugScreenSetXY(13, 7);
        pspDebugScreenPrintf("BETA EDITION");

        for (int i = 0; i < 5; i++) {
            pspDebugScreenSetXY(12, 10 + i * 2);
            pspDebugScreenSetTextColor(
                i == selecionado ? GOLD : WHITE
            );
            pspDebugScreenPrintf("%s %s",
                i == selecionado ? ">" : " ",
                opcoes[i]);
        }

        pspDebugScreenSetTextColor(GRAY);
        pspDebugScreenSetXY(3, 22);
        pspDebugScreenPrintf("CIMA/BAIXO: NAVEGAR  X: OK");

        if (pressionado & PSP_CTRL_UP)
            selecionado = (selecionado + 4) % 5;

        if (pressionado & PSP_CTRL_DOWN)
            selecionado = (selecionado + 1) % 5;

        if (pressionado & PSP_CTRL_CROSS) {
            if (selecionado == 0 || selecionado == 1) {
                player.x = 220.0f;
                player.y = 220.0f;
                player.vx = 0.0f;
                player.vy = 0.0f;
                player.grounded = 1;
                frame = 0;

                setup_game_graphics();
                tela = 1;
            } else if (selecionado == 4) {
                sair = 1;
            } else {
                tela = selecionado + 1;
            }
        }

        if (pressionado & PSP_CTRL_START)
            sair = 1;

        sceDisplayWaitVblankStart();
    } else if (tela == 1) {
        player.vx = 0.0f;
        animacao = 0;

        if (pad.Buttons & PSP_CTRL_LEFT) {
            player.vx = -3.0f;
            direcao = -1;
            animacao = 1;
        }

        if (pad.Buttons & PSP_CTRL_RIGHT) {
            player.vx = 3.0f;
            direcao = 1;
            animacao = 1;
        }

        if ((pad.Buttons & PSP_CTRL_RTRIGGER) &&
            player.grounded && player.vx != 0.0f) {
            player.vx *= 1.5f;
            animacao = 2;
        }

        if ((pressionado & PSP_CTRL_CROSS) &&
            player.grounded) {
            player.vy = -8.0f;
            player.grounded = 0;
        }

        player.vy += 0.35f;

        if (player.vy > 7.0f)
            player.vy = 7.0f;

        player.x += player.vx;
        player.y += player.vy;

        if (player.x < 0)
            player.x = 0;

        if (player.x > SCREEN_W - 32)
            player.x = SCREEN_W - 32;

        if (player.y >= 220.0f) {
            player.y = 220.0f;
            player.vy = 0.0f;
            player.grounded = 1;
        }

        Animation *current;

        if (!player.grounded)
            current = &jump_anim;
        else if (animacao == 2 && run_anim.count > 0)
            current = &run_anim;
        else if (animacao == 1)
            current = &walk_anim;
        else
            current = &idle_anim;

        if (current->count == 0)
            current = &idle_anim;

        if (current->count == 0)
            current = &walk_anim;

        if (current->count == 0)
            current = &run_anim;

        anim_timer++;
        if (anim_timer >= 7) {
            frame++;
            anim_timer = 0;
        }

        draw_game(&player, choose_frame(current, frame));

        if (pressionado & PSP_CTRL_CIRCLE) {
            sceGuDisplay(GU_FALSE);
            pspDebugScreenInit();
            tela = 0;
        }
    } else {
        pspDebugScreenClear();
        pspDebugScreenSetTextColor(GOLD);
        pspDebugScreenSetXY(10, 10);
        pspDebugScreenPrintf("%s", opcoes[tela - 1]);

        pspDebugScreenSetTextColor(WHITE);
        pspDebugScreenSetXY(5, 13);
        pspDebugScreenPrintf("AREA EM DESENVOLVIMENTO");

        pspDebugScreenSetXY(5, 18);
        pspDebugScreenPrintf("CIRCULO: VOLTAR");

        if (pressionado & PSP_CTRL_CIRCLE)
            tela = 0;

        sceDisplayWaitVblankStart();
    }
}

free_animation(&idle_anim);
free_animation(&walk_anim);
free_animation(&run_anim);
free_animation(&jump_anim);

sceKernelExitGame();
return 0;

}
