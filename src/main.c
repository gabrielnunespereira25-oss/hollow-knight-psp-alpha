
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("HK Beta Menu", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(void) {
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
        pspDebugScreenSetXY(7, 2);
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf("HOLLOW KNIGHT");

        pspDebugScreenSetXY(11, 4);
        pspDebugScreenPrintf("BETA PSP");

        if (tela == 0) {
            for (int i = 0; i < 5; i++) {
                pspDebugScreenSetXY(8, 8 + i * 2);

                if (i == selecionado) {
                    pspDebugScreenSetTextColor(0xFF80D8FF);
                    pspDebugScreenPrintf("> %s", opcoes[i]);
                } else {
                    pspDebugScreenSetTextColor(0xFFFFFFFF);
                    pspDebugScreenPrintf("  %s", opcoes[i]);
                }
            }

            pspDebugScreenSetXY(2, 20);
            pspDebugScreenSetTextColor(0xFFAAAAAA);
            pspDebugScreenPrintf("CIMA/BAIXO: navegar");

            pspDebugScreenSetXY(2, 21);
            pspDebugScreenPrintf("X: selecionar | START: sair");

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
            pspDebugScreenSetXY(4, 10);
            pspDebugScreenPrintf("%s", opcoes[tela - 1]);

            pspDebugScreenSetXY(3, 13);
            pspDebugScreenPrintf("Funcao sera adicionada");

            pspDebugScreenSetXY(3, 14);
            pspDebugScreenPrintf("nas proximas versoes.");

            pspDebugScreenSetXY(3, 18);
            pspDebugScreenPrintf("CIRCULO: voltar");

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
