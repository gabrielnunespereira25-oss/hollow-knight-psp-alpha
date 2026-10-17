
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("HK Beta Menu", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define BG_COLOR       0xFF101522
#define TEXT_COLOR     0xFFFFFFFF
#define SELECT_COLOR   0xFFFFD080
#define MUTED_COLOR    0xFFAAAAAA

int main(void)
{
    pspDebugScreenInit();

    pspDebugScreenSetBackColor(BG_COLOR);
    pspDebugScreenSetTextColor(TEXT_COLOR);

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

        /* Cabecalho */
        pspDebugScreenSetTextColor(MUTED_COLOR);
        pspDebugScreenSetXY(5, 2);
        pspDebugScreenPrintf("HALL0WNEST  /  PS202");

        pspDebugScreenSetTextColor(TEXT_COLOR);
        pspDebugScreenSetXY(7, 5);
        pspDebugScreenPrintf("HOLLOW KNIGHT");

        pspDebugScreenSetTextColor(SELECT_COLOR);
        pspDebugScreenSetXY(9, 7);
        pspDebugScreenPrintf("- BETA EDITION -");

        if (tela == 0) {
            /* Menu principal */
            for (int i = 0; i < 5; i++) {
                pspDebugScreenSetXY(8, 10 + i * 2);

                if (i == selecionado) {
                    pspDebugScreenSetTextColor(SELECT_COLOR);
                    pspDebugScreenPrintf("> %s <", opcoes[i]);
                } else {
                    pspDebugScreenSetTextColor(TEXT_COLOR);
                    pspDebugScreenPrintf("  %s", opcoes[i]);
                }
            }

            pspDebugScreenSetTextColor(MUTED_COLOR);
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
            /* Telas secundarias */
            pspDebugScreenSetTextColor(SELECT_COLOR);
            pspDebugScreenSetXY(7, 11);
            pspDebugScreenPrintf("%s", opcoes[tela - 1]);

            pspDebugScreenSetTextColor(TEXT_COLOR);
            pspDebugScreenSetXY(3, 14);
            pspDebugScreenPrintf("AREA EM DESENVOLVIMENTO");

            pspDebugScreenSetTextColor(MUTED_COLOR);
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
