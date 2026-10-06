/*
 * Contagem sequencial de objetos em matriz binaria (conectividade 8).
 *
 * Ideia:
 *   1. Percorre a matriz celula por celula.
 *   2. Ao achar um 1 ainda nao visitado, encontrou um NOVO objeto:
 *      conta +1 e "inunda" (flood fill) todas as celulas conectadas.
 *   3. A inundacao marca as celulas como visitadas, entao o mesmo
 *      objeto nunca e contado duas vezes.
 *
 * O flood fill e ITERATIVO (pilha explicita alocada no heap), e nao
 * recursivo: em matrizes grandes um unico objeto pode ter milhoes de
 * celulas, o que estouraria a pilha de chamadas do processo.
 *
 * A matriz e alocada dinamicamente e pode vir de:
 *   - um dos 5 casos obrigatorios embutidos no codigo, ou
 *   - um arquivo .txt informado com -f (formato descrito em le_arquivo).
 *
 * Compilar:
 *   cc -std=c89 -Wall -Wextra -pedantic -O2 src/sequencial.c -o build/sequencial
 */
#define _POSIX_C_SOURCE 199309L   /* necessario para clock_gettime em C89 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_DIM 20000             /* limite de linhas e de colunas       */
#define MAX_CELULAS 100000000L    /* limite de linhas * colunas (10^8)   */

static unsigned char *matriz;     /* 0 = fundo, 1 = objeto           */
static unsigned char *visitado;   /* 0 = nao visitada, 1 = visitada  */
static int *pilha;                /* pilha do flood fill (indices)   */
static int linhas, colunas;

/* Acesso a celula (l, c) de um vetor linear linhas x colunas. */
#define CEL(v, l, c) ((v)[(l) * colunas + (c)])

/* ---------------- Memoria da matriz ---------------- */

static void libera_matriz(void)
{
    free(matriz);
    free(visitado);
    free(pilha);
    matriz = NULL;
    visitado = NULL;
    pilha = NULL;
}

/* Aloca as estruturas para uma matriz l x c. Retorna 0 ou -1 (erro). */
static int aloca_matriz(int l, int c)
{
    size_t n;

    if (l < 1 || c < 1 || l > MAX_DIM || c > MAX_DIM ||
        (long)l * (long)c > MAX_CELULAS) {
        fprintf(stderr, "Dimensoes invalidas: %dx%d\n", l, c);
        return -1;
    }

    libera_matriz();
    n = (size_t)l * (size_t)c;
    matriz = (unsigned char *)malloc(n);
    visitado = (unsigned char *)malloc(n);
    pilha = (int *)malloc(n * sizeof(int));  /* cada celula entra 1 vez */
    if (matriz == NULL || visitado == NULL || pilha == NULL) {
        fprintf(stderr, "Memoria insuficiente para %dx%d\n", l, c);
        libera_matriz();
        return -1;
    }

    linhas = l;
    colunas = c;
    return 0;
}

/* ---------------- Contagem ---------------- */

/* Marca como visitadas todas as celulas ligadas a (l0, c0). */
static void inunda(int l0, int c0)
{
    int topo = 0, idx, l, c, dl, dc, nl, nc;

    /* a celula e marcada ao ENTRAR na pilha, entao nunca entra duas vezes */
    CEL(visitado, l0, c0) = 1;
    pilha[topo++] = l0 * colunas + c0;

    while (topo > 0) {
        idx = pilha[--topo];
        l = idx / colunas;
        c = idx % colunas;

        /* visita os 8 vizinhos (horizontal, vertical e diagonal) */
        for (dl = -1; dl <= 1; dl++)
            for (dc = -1; dc <= 1; dc++) {
                if (dl == 0 && dc == 0) continue;
                nl = l + dl;
                nc = c + dc;
                /* fora da matriz, fundo ou ja visitada: nada a fazer */
                if (nl < 0 || nl >= linhas || nc < 0 || nc >= colunas) continue;
                if (CEL(matriz, nl, nc) == 0) continue;
                if (CEL(visitado, nl, nc)) continue;

                CEL(visitado, nl, nc) = 1;
                pilha[topo++] = nl * colunas + nc;
            }
    }
}

/* Conta quantos objetos existem na matriz atual. */
static int conta_objetos(void)
{
    int l, c, total = 0;

    memset(visitado, 0, (size_t)linhas * (size_t)colunas);  /* zera a marcacao */

    for (l = 0; l < linhas; l++)
        for (c = 0; c < colunas; c++)
            if (CEL(matriz, l, c) == 1 && !CEL(visitado, l, c)) {
                total++;          /* achou um objeto novo */
                inunda(l, c);     /* marca ele inteiro    */
            }

    return total;
}

/* Tempo em milissegundos de um relogio monotonico. */
static double agora_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(1);
    }
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* ---------------- Entrada ---------------- */

/* Copia uma matriz estatica (vetor linear) para a matriz de trabalho. */
static int carrega(const int *dados, int l, int c)
{
    int i;

    if (aloca_matriz(l, c) != 0) return -1;
    for (i = 0; i < l * c; i++)
        matriz[i] = (unsigned char)dados[i];
    return 0;
}

/*
 * Le uma matriz de um arquivo texto. Formato:
 *   - linhas iniciadas por '#' sao comentarios (so antes do cabecalho);
 *   - cabecalho: "<linhas> <colunas> [objetos_esperados]";
 *   - depois, linhas*colunas valores 0 ou 1, separados ou nao por
 *     espacos, virgulas ou quebras de linha ("0 1 1" e "011" valem).
 * Em *esperado devolve o valor do cabecalho, ou -1 se ausente.
 * Retorna 0 ou -1 (erro, ja reportado).
 */
static int le_arquivo(const char *caminho, int *esperado)
{
    FILE *f;
    char buf[256];
    int l, c, ch, lidos;
    long i, n;

    f = fopen(caminho, "r");
    if (f == NULL) {
        perror(caminho);
        return -1;
    }

    /* pula comentarios e linhas vazias ate achar o cabecalho */
    do {
        if (fgets(buf, sizeof(buf), f) == NULL) {
            fprintf(stderr, "%s: cabecalho ausente\n", caminho);
            fclose(f);
            return -1;
        }
    } while (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r');

    *esperado = -1;
    lidos = sscanf(buf, "%d %d %d", &l, &c, esperado);
    if (lidos < 2) {
        fprintf(stderr, "%s: cabecalho invalido (esperado \"linhas colunas"
                " [objetos]\")\n", caminho);
        fclose(f);
        return -1;
    }
    if (lidos < 3) *esperado = -1;

    if (aloca_matriz(l, c) != 0) {
        fclose(f);
        return -1;
    }

    n = (long)l * (long)c;
    i = 0;
    while ((ch = getc(f)) != EOF) {
        if (ch == '0' || ch == '1') {
            if (i >= n) {
                fprintf(stderr, "%s: mais de %ld valores\n", caminho, n);
                fclose(f);
                return -1;
            }
            matriz[i++] = (unsigned char)(ch - '0');
        } else if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' &&
                   ch != ',') {
            fprintf(stderr, "%s: caractere invalido '%c' (posicao %ld)\n",
                    caminho, ch, i);
            fclose(f);
            return -1;
        }
    }
    fclose(f);

    if (i != n) {
        fprintf(stderr, "%s: esperados %ld valores, lidos %ld\n", caminho, n, i);
        return -1;
    }
    return 0;
}

/* ---------------- Matrizes obrigatorias ---------------- */

static const int ex1[] = {
    1,1,0,0,0,
    1,1,0,0,0,
    0,0,0,1,0,
    0,0,0,1,0,
    1,0,0,0,0
};

static const int ex2[] = {
    0,0,0,0,0,0,1,1,
    0,1,1,1,1,0,1,0,
    0,0,1,1,0,0,0,0,
    0,0,0,1,1,0,0,0,
    0,0,0,0,1,0,0,1,
    1,1,0,0,0,0,1,1
};

static const int ex3[] = {
    1,1,0,0,0,0,0,0,
    1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,0,
    0,0,0,1,1,0,1,0,
    0,0,0,1,1,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,1,
    0,0,1,0,0,0,1,1
};

static const int ex4[] = {
    0,1,1,0,0,0,0,0,0,0,1,0,
    0,0,1,1,1,1,0,0,0,1,1,0,
    0,0,0,0,0,1,0,0,0,0,0,0,
    0,0,0,0,0,1,1,0,0,0,0,0,
    0,1,0,0,0,0,1,0,0,1,0,0,
    0,1,1,0,0,0,0,0,1,1,0,0,
    0,0,1,1,0,0,0,0,1,0,0,0,
    0,0,0,1,0,0,0,1,1,0,0,0,
    0,0,0,0,0,1,0,0,0,0,0,1
};

static const int ex5[] = {
    1,0,0,0,0,1,1,1,1,0,1,1,
    0,1,0,0,0,1,0,0,1,0,1,0,
    0,0,1,0,0,0,0,0,0,0,0,0,
    0,0,0,1,0,0,0,0,0,0,0,0,
    0,0,0,0,1,0,0,0,0,0,0,0,
    1,1,0,0,0,1,0,0,0,0,0,0,
    1,0,0,0,0,0,1,0,0,0,0,0,
    0,0,0,1,1,0,0,1,0,0,0,0,
    0,0,0,1,1,0,0,0,1,0,0,0,
    0,0,0,0,0,0,0,0,0,1,0,0,
    0,1,0,0,0,0,0,0,0,0,1,0,
    0,1,1,0,0,0,1,0,0,0,0,1
};

/* ---------------- Mock dos casos de teste ---------------- */

struct caso_teste {
    const char *nome;
    const int *dados;
    int linhas;
    int colunas;
    int esperado;
};

static const struct caso_teste casos[] = {
    { "Identificacao basica",                      ex1,  5,  5, 3 },
    { "Objeto atravessando fronteiras",            ex2,  6,  8, 4 },
    { "Encontro de quatro blocos e diagonal",      ex3,  8,  8, 5 },
    { "Objetos irregulares em varios blocos",      ex4,  9, 12, 6 },
    { "Matriz maior com travessia diagonal",       ex5, 12, 12, 7 }
};

#define NUM_CASOS ((int)(sizeof(casos) / sizeof(casos[0])))

/* Executa o caso numero n (1..NUM_CASOS). Retorna 1 se passou, 0 se falhou. */
static int executa_caso(int n)
{
    const struct caso_teste *t = &casos[n - 1];
    int obtido;

    if (carrega(t->dados, t->linhas, t->colunas) != 0) return 0;
    obtido = conta_objetos();

    printf("Ex%d - %s (%dx%d): obtido=%d esperado=%d [%s]\n",
           n, t->nome, t->linhas, t->colunas, obtido, t->esperado,
           obtido == t->esperado ? "OK" : "FALHOU");

    return obtido == t->esperado;
}

/*
 * Executa a matriz de um arquivo. So a contagem e cronometrada (a
 * leitura do arquivo fica de fora). Retorna 1 se passou, 0 se falhou.
 */
static int executa_arquivo(const char *caminho)
{
    int obtido, esperado;
    double ini, fim;

    if (le_arquivo(caminho, &esperado) != 0) return 0;

    ini = agora_ms();
    obtido = conta_objetos();
    fim = agora_ms();

    printf("%s (%dx%d): obtido=%d", caminho, linhas, colunas, obtido);
    if (esperado >= 0)
        printf(" esperado=%d [%s]", esperado,
               obtido == esperado ? "OK" : "FALHOU");
    printf(" tempo_ms=%.3f\n", fim - ini);

    return esperado < 0 || obtido == esperado;
}

static void uso(const char *prog)
{
    printf("Uso: %s <caso>\n", prog);
    printf("     %s -f <arquivo.txt>\n", prog);
    printf("  <caso> = 1..%d  executa um caso especifico\n", NUM_CASOS);
    printf("  <caso> = todos  executa todos os casos\n");
    printf("  <caso> = lista  lista os casos disponiveis\n");
    printf("  -f <arquivo>    le a matriz de um arquivo texto\n");
}

int main(int argc, char *argv[])
{
    int i, n, ok = 1;

    if (argc == 3 && strcmp(argv[1], "-f") == 0) {
        ok = executa_arquivo(argv[2]);
        libera_matriz();
        return ok ? 0 : 1;
    }

    if (argc != 2) {
        uso(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "lista") == 0) {
        for (i = 0; i < NUM_CASOS; i++)
            printf("%d: %s (%dx%d, esperado %d)\n", i + 1, casos[i].nome,
                   casos[i].linhas, casos[i].colunas, casos[i].esperado);
        return 0;
    }

    if (strcmp(argv[1], "todos") == 0) {
        for (i = 1; i <= NUM_CASOS; i++)
            if (!executa_caso(i)) ok = 0;
        libera_matriz();
        return ok ? 0 : 1;
    }

    n = atoi(argv[1]);
    if (n < 1 || n > NUM_CASOS) {
        printf("Caso invalido: %s\n", argv[1]);
        uso(argv[0]);
        return 1;
    }

    ok = executa_caso(n);
    libera_matriz();
    return ok ? 0 : 1;
}
