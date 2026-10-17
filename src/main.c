
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("Hollow Knight PSP Beta", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define FLOOR_Y 235

typedef enum {
    IDLE,
    WALK,
    RUN,
    JUMP,
    FALL,
    LAND,
    ATTACK,
    HIT,
    DIE
} Animation;

typedef struct {
    float x, y, vy;
    int grounded;
    int direction;
    Animation animation;
    int frame;
    int frame_timer;
} Player;

static Player player = {80, FLOOR_Y - 48, 0, 1, 1, IDLE, 0, 0};

static const char *idle_frames[] = {
    "ativos/parado/idle_00.png",
    "ativos/parado/idle_01.png",
    "ativos/parado/idle_02.png",
    "ativos/parado/idle_03.png"
};

static const char *walk_frames[] = {
    "ativos/andar/caminhada_00.png",
    "ativos/andar/caminhada_01.png",
    "ativos/andar/caminhada_02.png",
    "ativos/andar/caminhada_03.png",
    "ativos/andar/caminhada_04.png",
    "ativos/andar/caminhada_05.png"
};

static const char *run_frames[] = {
    "ativos/correr/run_00.png",
    "ativos/correr/run_01.png",
    "ativos/correr/run_02.png",
    "ativos/correr/run_03.png",
    "ativos/correr/run_04.png",
    "ativos/correr/run_05.png"
};

static const char *jump_frames[] = {
    "ativos/pular/jump_00.png",
    "ativos/pular/jump_01.png",
    "ativos/pular/jump_02.png",
    "ativos/pular/jump_03.png",
    "ativos/pular/jump_04.png",
    "ativos/pular/jump_05.png"
};

static const char *hit_frames[] = {
    "ativos/bater/hit_00.png",
    "ativos/bater/hit_01.png",
    "ativos/bater/hit_02.png",
    "ativos/bater/hit_03.png"
};

static void set_animation(Animation animation)
{
    if (player.animation != animation) {
        player.animation = animation;
        player.frame = 0;
        player.frame_timer = 0;
    }
}

static void update_player(SceCtrlData *pad)
{
    unsigned int b = pad->Buttons;
    int moving = 0;

    if (b & PSP_CTRL_LEFT) {
        player.x -= 2.0f;
        player.direction = -1;
        moving = 1;
    }

    if (b & PSP_CTRL_RIGHT) {
        player.x += 2.0f;
        player.direction = 1;
        moving = 1;
    }

    if (player.x < 0)
        player.x = 0;

    if (player.x > SCREEN_W - 32)
        player.x = SCREEN_W - 32;

    if ((b & PSP_CTRL_CROSS) && player.grounded) {
        player.vy = -7.0f;
        player.grounded = 0;
        set_animation(JUMP);
    }

    player.vy += 0.30f;
    player.y += player.vy;

    if (player.y + 48 >= FLOOR_Y) {
        player.y = FLOOR_Y - 48;
        player.vy = 0;
        player.grounded = 1;
    }

    if (b & PSP_CTRL_SQUARE)
        set_animation(ATTACK);
    else if (!player.grounded)
        set_animation(player.vy < 0 ? JUMP : FALL);
    else if (moving)
        set_animation(WALK);
    else
        set_animation(IDLE);

    player.frame_timer++;

    if (player.frame_timer >= 8) {
        player.frame_timer = 0;
        player.frame++;
    }
}

static void draw_placeholder(void)
{
    /*
     * PONTO DE INTEGRACAO:
     * Aqui entra o carregador PNG + textura PSP GU.
     *
     * Ainda não desenha os arquivos PNG.
     * Este retângulo serve apenas como marcador temporário.
     */
    (void)idle_frames;
    (void)walk_frames;
    (void)run_frames;
    (void)jump_frames;
    (void)hit_frames;
}

int main(void)
{
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    pspDebugScreenInit();

    pspDebugScreenPrintf("HOLLOW KNIGHT PSP BETA\n");
    pspDebugScreenPrintf("Prototipo de animacao\n");
    pspDebugScreenPrintf("Esquerda/Direita: mover\n");
    pspDebugScreenPrintf("X: pular | Quadrado: atacar\n");

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        update_player(&pad);
        draw_placeholder();

        sceDisplayWaitVblankStart();
        sceKernelDelayThread(16000);
    }

    sceKernelExitGame();
    return 0;
}
