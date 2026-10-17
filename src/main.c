
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("HK Beta Menu", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define WHITE  0xFFFFFFFF
#define GOLD   0xFFFFD080
#define GRAY   0xFFAAAAAA

int main(void)
{
    pspDebugScreenInit();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    int selecionado = 0;
    int tela = 0;
    int sair = 0;

    const char *opcoes[] = {
        "NOVO JOGO",
        "CONTINUAR",
        "OPCOES",
        "CREDITOS",
        "SAIR"
    };

    while (!sair) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        pspDebugScreenClear();

        pspDebugScreenSetTextColor(GRAY);
        pspDebugScreenSetXY(5, 2);
        pspDebugScreenPrintf("HOLLOW NEST / PS202");

        pspDebugScreenSetTextColor(WHITE);
        pspDebugScreenSetXY(7, 5);
        pspDebugScreenPrintf("HOLLOW KNIGHT");

        pspDebugScreenSetTextColor(GOLD);
        pspDebugScreenSetXY(9, 7);
        pspDebugScreenPrintf("BETA EDITION");

        if (tela == 0) {
            for (int i = 0; i < 5; i++) {
                pspDebugScreenSetXY(8, 10 + i * 2);

                if (i == selecionado) {
                    pspDebugScreenSetTextColor(GOLD);
                    pspDebugScreenPrintf("> %s <", opcoes[i]);
                } else {
                    pspDebugScreenSetTextColor(WHITE);
                    pspDebugScreenPrintf("  %s", opcoes[i]);
                }
            }

            pspDebugScreenSetTextColor(GRAY);
            pspDebugScreenSetXY(3, 21);
            pspDebugScreenPrintf("CIMA/BAIXO: NAVEGAR");

            pspDebugScreenSetXY(3, 22);
            pspDebugScreenPrintf("X: SELECIONAR  START: SAIR");

            if (pad.Buttons & PSP_CTRL_UP) {
                selecionado = (selecionado + 4) % 5;
                sceKernelDelayThread(180000);
            }

            if (pad.Buttons & PSP_CTRL_DOWN) {
                selecionado = (selecionado + 1) % 5;
                sceKernelDelayThread(180000);
            }

            if (pad.Buttons & PSP_CTRL_CROSS) {
                if (selecionado == 4) {
                    sair = 1;
                } else {
                    tela = selecionado + 1;
                }

                sceKernelDelayThread(200000);
            }

            if (pad.Buttons & PSP_CTRL_START) {
                sair = 1;
            }
        } else {
            pspDebugScreenSetTextColor(GOLD);
            pspDebugScreenSetXY(7, 11);
            pspDebugScreenPrintf("%s", opcoes[tela - 1]);

            pspDebugScreenSetTextColor(WHITE);
            pspDebugScreenSetXY(3, 14);
            pspDebugScreenPrintf("AREA EM DESENVOLVIMENTO");

            pspDebugScreenSetTextColor(GRAY);
            pspDebugScreenSetXY(5, 19);
            pspDebugScreenPrintf("CIRCULO: VOLTAR");

            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                tela = 0;
                sceKernelDelayThread(200000);
            }
        }

        sceDisplayWaitVblankStart();
    }

    sceKernelExitGame();
    return 0;
}
