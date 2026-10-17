
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <string.h>

PSP_MODULE_INFO("Hollow Knight PSP Beta", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define LARGURA 480
#define ALTURA  272
#define CHAO_Y  235

#define PRETO   0xFF101018
#define CINZA   0xFF777783
#define CLARO   0xFFD8D8D0
#define BRANCO  0xFFFFFFFF
#define AZUL    0xFF252B45
#define ROXO    0xFF393448

static unsigned int listaGU[262144]
    __attribute__((aligned(16)));

typedef struct {
    unsigned int cor;
    float x, y, z;
} Vertice;

typedef struct {
    float x, y;
    float vx, vy;
    int noChao;
    int atacando;
    int direcao;
} Jogador;

static Jogador jogador;
static int estado = 0; /* 0 = menu, 1 = jogo */
static int sair = 0;
static int opcao = 0;
static int ataqueFrames = 0;

static const char *opcoes[] = {
    "NOVO JOGO",
    "CONTINUAR",
    "OPCOES",
    "CREDITOS",
    "SAIR"
};

/* Desenha um retangulo usando o PSP GU. */
static void retangulo(
    float x, float y, float largura, float altura,
    unsigned int cor)
{
    Vertice v[2];

    v[0].cor = cor;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].cor = cor;
    v[1].x = x + largura;
    v[1].y = y + altura;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v
    );
}

/* Inicializa o modo grafico 2D. */
static void iniciarGraficos(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, listaGU);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        512
    );

    sceGuDispBuffer(
        LARGURA,
        ALTURA,
        (void *)0x88000,
        512
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        512
    );

    sceGuOffset(2048 - (LARGURA / 2),
                2048 - (ALTURA / 2));

    sceGuViewport(2048, 2048, LARGURA, ALTURA);

    sceGuScissor(0, 0, LARGURA, ALTURA);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_FLAT);

    sceGuClearColor(PRETO);
    sceGuClear(GU_COLOR_BUFFER_BIT |
               GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* Inicia uma nova partida. */
static void novoJogo(void)
{
    jogador.x = 80.0f;
    jogador.y = CHAO_Y - 48.0f;
    jogador.vx = 0.0f;
    jogador.vy = 0.0f;
    jogador.noChao = 1;
    jogador.atacando = 0;
    jogador.direcao = 1;
    ataqueFrames = 0;
    estado = 1;
}

/*
 * Personagem provisoriamente desenhado por formas.
 * O sprite PNG sera integrado numa etapa posterior.
 */
static void desenharJogador(void)
{
    float x = jogador.x;
    float y = jogador.y;

    /* Sombra no chao. */
    retangulo(x - 5, CHAO_Y - 3, 34, 4, PRETO);

    /* Pernas e botas. */
    retangulo(x + 5, y + 36, 7, 11, PRETO);
    retangulo(x + 18, y + 36, 7, 11, PRETO);
    retangulo(x + 3, y + 44, 11, 4, CINZA);
    retangulo(x + 17, y + 44, 12, 4, CINZA);

    /* Capa. */
    retangulo(x + 1, y + 20, 27, 19, PRETO);
    retangulo(x + 4, y + 22, 21, 14, ROXO);
    retangulo(x, y + 34, 8, 5, ROXO);
    retangulo(x + 21, y + 34, 8, 5, ROXO);

    /* Cabeca. */
    retangulo(x + 5, y + 7, 20, 19, PRETO);
    retangulo(x + 8, y + 10, 14, 13, CINZA);

    /* Chifres curvados estilizados. */
    retangulo(x + 3, y + 2, 5, 11, CLARO);
    retangulo(x + 3, y + 2, 9, 4, CLARO);
    retangulo(x + 22, y + 2, 5, 11, CLARO);
    retangulo(x + 18, y + 2, 9, 4, CLARO);

    /* Olhos claros. */
    retangulo(x + 9, y + 15, 4, 5, BRANCO);
    retangulo(x + 17, y + 15, 4, 5, BRANCO);

    /* Ataque provisoriamente representado por uma lamina. */
    if (jogador.atacando) {
        if (jogador.direcao > 0) {
            retangulo(x + 28, y + 17, 20, 4, BRANCO);
            retangulo(x + 36, y + 13, 4, 12, CLARO);
        } else {
            retangulo(x - 20, y + 17, 20, 4, BRANCO);
            retangulo(x - 16, y + 13, 4, 12, CLARO);
        }
    }
}

/* Desenha o cenario provisório. */
static void desenharCenario(void)
{
    int i;

    /* Fundo escuro. */
    retangulo(0, 0, LARGURA, ALTURA, AZUL);

    /* Silhuetas simples ao fundo. */
    retangulo(25, 95, 35, 80, PRETO);
    retangulo(75, 120, 24, 55, PRETO);
    retangulo(350, 90, 35, 85, PRETO);
    retangulo(410, 115, 24, 60, PRETO);

    /* Plataformas e chao. */
    retangulo(0, CHAO_Y, LARGURA, ALTURA - CHAO_Y, PRETO);
    retangulo(0, CHAO_Y, LARGURA, 5, CINZA);

    for (i = 0; i < LARGURA; i += 24) {
        retangulo((float)i, CHAO_Y + 12, 1, 10, ROXO);
    }
}

/* Atualiza a fisica e os controles. */
static void atualizarJogo(SceCtrlData *pad)
{
    const unsigned int botoes = pad->Buttons;

    jogador.vx = 0.0f;

    if (botoes & PSP_CTRL_LEFT) {
        jogador.vx = -2.5f;
        jogador.direcao = -1;
    }

    if (botoes & PSP_CTRL_RIGHT) {
        jogador.vx = 2.5f;
        jogador.direcao = 1;
    }

    jogador.x += jogador.vx;

    if (jogador.x < 5.0f)
        jogador.x = 5.0f;

    if (jogador.x > LARGURA - 35.0f)
        jogador.x = LARGURA - 35.0f;

    /* Pulo: X. */
    if ((botoes & PSP_CTRL_CROSS) && jogador.noChao) {
        jogador.vy = -7.5f;
        jogador.noChao = 0;
    }

    /* Gravidade. */
    jogador.vy += 0.30f;

    if (jogador.vy > 6.0f)
        jogador.vy = 6.0f;

    jogador.y += jogador.vy;

    /* Colisao com o chao. */
    if (jogador.y + 48.0f >= CHAO_Y) {
        jogador.y = CHAO_Y - 48.0f;
        jogador.vy = 0.0f;
        jogador.noChao = 1;
    }

    /* Ataque: Quadrado. */
    if (botoes & PSP_CTRL_SQUARE) {
        jogador.atacando = 1;
        ataqueFrames = 8;
    }

    if (ataqueFrames > 0) {
        ataqueFrames--;
        if (ataqueFrames == 0)
            jogador.atacando = 0;
    }

    /* Circulo volta ao menu. */
    if (botoes & PSP_CTRL_CIRCLE) {
        estado = 0;
        jogador.atacando = 0;
    }
}

/* Tela provisoria do menu. */
static void desenharMenu(void)
{
    int i;

    pspDebugScreenInit();
    pspDebugScreenSetTextColor(CLARO);
    pspDebugScreenSetXY(5, 3);
    pspDebugScreenPrintf("HOLLOW KNIGHT PSP BETA");

    pspDebugScreenSetTextColor(CINZA);
    pspDebugScreenSetXY(7, 5);
    pspDebugScreenPrintf("CRIATURA DAS SOMBRAS");

    for (i = 0; i < 5; i++) {
        pspDebugScreenSetXY(9, 9 + i * 2);

        if (i == opcao) {
            pspDebugScreenSetTextColor(CLARO);
            pspDebugScreenPrintf("> %s", opcoes[i]);
        } else {
            pspDebugScreenSetTextColor(BRANCO);
            pspDebugScreenPrintf("  %s", opcoes[i]);
        }
    }

    pspDebugScreenSetTextColor(CINZA);
    pspDebugScreenSetXY(4, 23);
    pspDebugScreenPrintf("DIRECIONAL: NAVEGAR  X: SELECIONAR");
    pspDebugScreenSetXY(4, 24);
    pspDebugScreenPrintf("START: SAIR");
}

int main(void)
{
    SceCtrlData pad;
    unsigned int anterior = 0;
    int menuInicializado = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    jogador.x = 80.0f;
    jogador.y = CHAO_Y - 48.0f;
    jogador.vx = 0.0f;
    jogador.vy = 0.0f;
    jogador.noChao = 1;
    jogador.atacando = 0;
    jogador.direcao = 1;

    while (!sair) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (estado == 0) {
            if (!menuInicializado) {
                desenharMenu();
                menuInicializado = 1;
            }

            if ((pad.Buttons & PSP_CTRL_UP) &&
                !(anterior & PSP_CTRL_UP)) {
                opcao = (opcao + 4) % 5;
                desenharMenu();
            }

            if ((pad.Buttons & PSP_CTRL_DOWN) &&
                !(anterior & PSP_CTRL_DOWN)) {
                opcao = (opcao + 1) % 5;
                desenharMenu();
            }

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(anterior & PSP_CTRL_CROSS)) {
                if (opcao == 0 || opcao == 1) {
                    iniciarGraficos();
                    novoJogo();
                    menuInicializado = 0;
                } else if (opcao == 4) {
                    sair = 1;
                } else {
                    /* Opcoes e creditos ainda nao implementados. */
                    pspDebugScreenSetXY(4, 21);
                    pspDebugScreenPrintf("EM DESENVOLVIMENTO");
                }
            }

            if ((pad.Buttons & PSP_CTRL_START) &&
                !(anterior & PSP_CTRL_START)) {
                sair = 1;
            }
        } else {
            atualizarJogo(&pad);

            sceGuStart(GU_DIRECT, listaGU);
            sceGuClearColor(PRETO);
            sceGuClear(GU_COLOR_BUFFER_BIT |
                       GU_DEPTH_BUFFER_BIT);

            desenharCenario();
            desenharJogador();

            sceGuFinish();
            sceGuSync(0, 0);
            sceDisplayWaitVblankStart();
            sceGuSwapBuffers();

            if (estado == 0) {
                menuInicializado = 0;
            }
        }

        anterior = pad.Buttons;
        sceKernelDelayThread(16000);
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
