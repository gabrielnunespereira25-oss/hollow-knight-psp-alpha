
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>

PSP_MODULE_INFO("Dark Bot Test", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUFFER_W 512

static unsigned int __attribute__((aligned(16)))
    display_list[262144];

int main(void)
{
    SceCtrlData pad;
    int running = 1;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    sceGuInit();
    sceGuStart(GU_DIRECT, display_list);

    sceGuDrawBuffer(
        GU_PSM_8888, (void *)0, BUFFER_W
    );

    sceGuDispBuffer(
        SCREEN_W, SCREEN_H, (void *)0x88000, BUFFER_W
    );

    sceGuOffset(
        2048 - SCREEN_W / 2,
        2048 - SCREEN_H / 2
    );

    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    while (running)
    {
        sceGuStart(GU_DIRECT, display_list);

        sceGuClearColor(0xFF0000FF);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_CIRCLE)
            running = 0;
    }

    sceGuDisplay(GU_FALSE);
    sceGuTerm();

    sceKernelExitGame();
    return 0;
}
