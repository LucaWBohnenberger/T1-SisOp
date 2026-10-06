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
 * Compilar:
 *   cc -std=c89 -Wall -Wextra -pedantic conta-objetos-sequencial.c -o sequencial
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX 100

static int matriz[MAX][MAX];     /* 0 = fundo, 1 = objeto           */
static int visitado[MAX][MAX];   /* 0 = nao visitada, 1 = visitada  */
static int linhas, colunas;

/* Marca como visitadas todas as celulas ligadas a (l, c). */
static void inunda(int l, int c)
{
    int dl, dc;

    /* fora da matriz, fundo ou ja visitada: nada a fazer */
    if (l < 0 || l >= linhas || c < 0 || c >= colunas) return;
    if (matriz[l][c] == 0) return;
    if (visitado[l][c]) return;

    visitado[l][c] = 1;

    /* visita os 8 vizinhos (horizontal, vertical e diagonal) */
    for (dl = -1; dl <= 1; dl++)
        for (dc = -1; dc <= 1; dc++)
            if (dl != 0 || dc != 0)
                inunda(l + dl, c + dc);
}

/* Conta quantos objetos existem na matriz atual. */
static int conta_objetos(void)
{
    int l, c, total = 0;

    for (l = 0; l < linhas; l++)
        for (c = 0; c < colunas; c++) {
            visitado[l][c] = 0;   /* zera a marcacao */
        }

    for (l = 0; l < linhas; l++)
        for (c = 0; c < colunas; c++)
            if (matriz[l][c] == 1 && !visitado[l][c]) {
                total++;          /* achou um objeto novo */
                inunda(l, c);     /* marca ele inteiro    */
            }

    return total;
}

/* Copia uma matriz estatica (vetor linear) para a matriz de trabalho. */
static void carrega(const int *dados, int l, int c)
{
    int i, j;
    linhas = l;
    colunas = c;
    for (i = 0; i < l; i++)
        for (j = 0; j < c; j++)
            matriz[i][j] = dados[i * c + j];
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

    carrega(t->dados, t->linhas, t->colunas);
    obtido = conta_objetos();

    printf("Ex%d - %s (%dx%d): obtido=%d esperado=%d [%s]\n",
           n, t->nome, t->linhas, t->colunas, obtido, t->esperado,
           obtido == t->esperado ? "OK" : "FALHOU");

    return obtido == t->esperado;
}

static void uso(const char *prog)
{
    printf("Uso: %s <caso>\n", prog);
    printf("  <caso> = 1..%d  executa um caso especifico\n", NUM_CASOS);
    printf("  <caso> = todos  executa todos os casos\n");
    printf("  <caso> = lista  lista os casos disponiveis\n");
}

int main(int argc, char *argv[])
{
    int i, n, ok = 1;

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
        return ok ? 0 : 1;
    }

    n = atoi(argv[1]);
    if (n < 1 || n > NUM_CASOS) {
        printf("Caso invalido: %s\n", argv[1]);
        uso(argv[0]);
        return 1;
    }

    return executa_caso(n) ? 0 : 1;
}