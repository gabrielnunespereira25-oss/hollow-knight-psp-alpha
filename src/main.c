
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>

PSP_MODULE_INFO("Dark Bot", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define WHITE 0xFFFFFFFF
#define GOLD  0xFFFFD080
#define GRAY  0xFFAAAAAA

#define SCREEN_W 480
#define SCREEN_H 272

typedef struct {
    float x, y;
    float vx, vy;
    int grounded;
} Player;

int main(void)
{
    pspDebugScreenInit();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    int selecionado = 0;
    int tela = 0;
    int sair = 0;
    unsigned int anterior = 0;

    Player player = {220.0f, 180.0f, 0.0f, 0.0f, 1};

    const char *opcoes[] = {
        "NOVO JOGO", "CONTINUAR", "OPCOES", "CREDITOS", "SAIR"
    };

    while (!sair) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int pressionado = pad.Buttons & ~anterior;
        anterior = pad.Buttons;

        pspDebugScreenClear();

        if (tela == 0) {
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
                    player.y = 180.0f;
                    player.vx = 0.0f;
                    player.vy = 0.0f;
                    player.grounded = 1;
                    tela = 1;
                } else if (selecionado == 4) {
                    sair = 1;
                } else {
                    tela = selecionado + 1;
                }
            }

            if (pressionado & PSP_CTRL_START)
                sair = 1;

        } else if (tela == 1) {
            /* Movimento horizontal */
            player.vx = 0.0f;

            if (pad.Buttons & PSP_CTRL_LEFT)
                player.vx = -3.0f;

            if (pad.Buttons & PSP_CTRL_RIGHT)
                player.vx = 3.0f;

            /* Pulo com X */
            if ((pressionado & PSP_CTRL_CROSS) && player.grounded) {
                player.vy = -8.0f;
                player.grounded = 0;
            }

            /* Gravidade */
            player.vy += 0.35f;
            if (player.vy > 7.0f)
                player.vy = 7.0f;

            player.x += player.vx;
            player.y += player.vy;

            /* Limites da tela */
            if (player.x < 0)
                player.x = 0;

            if (player.x > SCREEN_W - 16)
                player.x = SCREEN_W - 16;

            /* Chão de teste */
            if (player.y >= 220.0f) {
                player.y = 220.0f;
                player.vy = 0.0f;
                player.grounded = 1;
            }

            pspDebugScreenSetTextColor(WHITE);
            pspDebugScreenSetXY(2, 1);
            pspDebugScreenPrintf("DARK BOT - TESTE DE MOVIMENTO");

            pspDebugScreenSetXY(2, 2);
            pspDebugScreenPrintf("ESQ/DIR: ANDAR  X: PULAR");

            pspDebugScreenSetXY(2, 3);
            pspDebugScreenPrintf("CIRCULO: MENU");

            /* Representacao temporaria do personagem */
            pspDebugScreenSetTextColor(GOLD);
            pspDebugScreenSetXY((int)(player.x / 8),
                                (int)(player.y / 8));
            pspDebugScreenPrintf("[]");

            pspDebugScreenSetTextColor(GRAY);
            pspDebugScreenSetXY(0, 28);
            pspDebugScreenPrintf(
                "----------------------------------------------"
            );

            if (pressionado & PSP_CTRL_CIRCLE)
                tela = 0;

        } else {
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
        }

        sceDisplayWaitVblankStart();
    }

    sceKernelExitGame();
    return 0;
}
