/*
 * Contagem PARALELA de objetos em matriz binaria (conectividade 8).
 * Versao basica com Pthreads e divisao da matriz em FAIXAS DE LINHAS.
 *
 * Estrategia (3 fases):
 *
 *   FASE 1 - PARALELA: a matriz e dividida em faixas de linhas. Cada thread
 *            cuida de uma faixa e faz flood fill SOMENTE dentro dela,
 *            rotulando as celulas. Cada objeto local recebe como rotulo o
 *            numero da sua celula inicial (linha * colunas + coluna + 1),
 *            que e unico na matriz inteira. Como cada thread escreve apenas
 *            nas suas linhas, nao ha condicao de corrida.
 *
 *   FASE 2 - SEQUENCIAL (thread principal, apos os joins): total = soma das
 *            contagens locais. Mas um objeto que atravessa duas faixas foi
 *            contado duas vezes! Entao olhamos as linhas de fronteira entre
 *            faixas vizinhas: se duas celulas 1 se tocam (inclusive na
 *            diagonal) com rotulos diferentes, sao o mesmo objeto. Unimos os
 *            rotulos (union-find) e subtraimos 1 do total a cada uniao.
 *
 *   O pthread_join funciona como a sincronizacao: a fase 2 so comeca quando
 *   todas as threads terminaram, entao nenhum dado e lido antes de pronto.
 *
 * Compilar:
 *   cc -std=c89 -Wall -Wextra -pedantic -pthread conta-objetos-paralelo.c -o paralelo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX 100
#define MAX_THREADS 64

static int matriz[MAX][MAX];        /* 0 = fundo, 1 = objeto                */
static int rotulo[MAX][MAX];        /* 0 = sem rotulo, senao id do objeto   */
static int pai[MAX * MAX + 1];      /* union-find sobre os rotulos          */
static int linhas, colunas;

/* Trabalho entregue a cada thread. */
struct tarefa {
    int ini;       /* primeira linha da faixa (inclusive) */
    int fim;       /* ultima linha da faixa (exclusive)   */
    int objetos;   /* resultado: objetos locais da faixa  */
};

/* ---------------- FASE 1: flood fill dentro da faixa ---------------- */

static void inunda(int l, int c, int id, int ini, int fim)
{
    int dl, dc;

    /* nao sai da faixa [ini, fim) nem da matriz */
    if (l < ini || l >= fim || c < 0 || c >= colunas) return;
    if (matriz[l][c] == 0) return;
    if (rotulo[l][c] != 0) return;      /* ja rotulada */

    rotulo[l][c] = id;

    for (dl = -1; dl <= 1; dl++)
        for (dc = -1; dc <= 1; dc++)
            if (dl != 0 || dc != 0)
                inunda(l + dl, c + dc, id, ini, fim);
}

/* Funcao executada por cada thread. */
static void *trabalhador(void *arg)
{
    struct tarefa *t = (struct tarefa *)arg;
    int l, c;

    t->objetos = 0;
    for (l = t->ini; l < t->fim; l++)
        for (c = 0; c < colunas; c++)
            if (matriz[l][c] == 1 && rotulo[l][c] == 0) {
                t->objetos++;
                inunda(l, c, l * colunas + c + 1, t->ini, t->fim);
            }

    return NULL;
}

/* ---------------- FASE 2: union-find ---------------- */

static int acha(int x)
{
    while (pai[x] != x) x = pai[x];
    return x;
}

/* Une dois rotulos. Retorna 1 se eram objetos diferentes, 0 se ja iguais. */
static int uniao(int a, int b)
{
    a = acha(a);
    b = acha(b);
    if (a == b) return 0;
    pai[b] = a;
    return 1;
}

/* Conta objetos usando nthreads threads. Retorna -1 em caso de erro. */
static int conta_objetos(int nthreads)
{
    pthread_t tid[MAX_THREADS];
    struct tarefa tarefas[MAX_THREADS];
    int base, resto, ini, i, l, c, dc, c2, total, erro;

    if (nthreads > linhas) nthreads = linhas;   /* no minimo 1 linha/thread */

    /* zera rotulos e inicializa o union-find */
    memset(rotulo, 0, sizeof(rotulo));
    for (i = 0; i <= linhas * colunas; i++) pai[i] = i;

    /* divide as linhas em faixas (as primeiras faixas levam o resto) */
    base = linhas / nthreads;
    resto = linhas % nthreads;
    ini = 0;
    for (i = 0; i < nthreads; i++) {
        tarefas[i].ini = ini;
        tarefas[i].fim = ini + base + (i < resto ? 1 : 0);
        ini = tarefas[i].fim;
    }

    /* FASE 1: cria as threads */
    for (i = 0; i < nthreads; i++) {
        erro = pthread_create(&tid[i], NULL, trabalhador, &tarefas[i]);
        if (erro != 0) {
            fprintf(stderr, "pthread_create falhou: %s\n", strerror(erro));
            exit(1);
        }
    }

    /* espera todas terminarem */
    for (i = 0; i < nthreads; i++) {
        erro = pthread_join(tid[i], NULL);
        if (erro != 0) {
            fprintf(stderr, "pthread_join falhou: %s\n", strerror(erro));
            exit(1);
        }
    }

    /* FASE 2: soma local e correcao nas fronteiras */
    total = 0;
    for (i = 0; i < nthreads; i++) total += tarefas[i].objetos;

    for (i = 0; i < nthreads - 1; i++) {
        l = tarefas[i].fim - 1;               /* ultima linha da faixa i  */
        for (c = 0; c < colunas; c++) {
            if (rotulo[l][c] == 0) continue;
            /* vizinhos da linha de baixo: esquerda, centro, direita */
            for (dc = -1; dc <= 1; dc++) {
                c2 = c + dc;
                if (c2 < 0 || c2 >= colunas) continue;
                if (rotulo[l + 1][c2] != 0)
                    if (uniao(rotulo[l][c], rotulo[l + 1][c2]))
                        total--;              /* eram o mesmo objeto */
            }
        }
    }

    return total;
}

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
    { "Identificacao basica",                 ex1,  5,  5, 3 },
    { "Objeto atravessando fronteiras",       ex2,  6,  8, 4 },
    { "Encontro de quatro blocos e diagonal", ex3,  8,  8, 5 },
    { "Objetos irregulares em varios blocos", ex4,  9, 12, 6 },
    { "Matriz maior com travessia diagonal",  ex5, 12, 12, 7 }
};

#define NUM_CASOS ((int)(sizeof(casos) / sizeof(casos[0])))

static int executa_caso(int n, int nthreads)
{
    const struct caso_teste *t = &casos[n - 1];
    int obtido;

    carrega(t->dados, t->linhas, t->colunas);
    obtido = conta_objetos(nthreads);

    printf("Ex%d - %s (%dx%d) [%d threads]: obtido=%d esperado=%d [%s]\n",
           n, t->nome, t->linhas, t->colunas, nthreads, obtido, t->esperado,
           obtido == t->esperado ? "OK" : "FALHOU");

    return obtido == t->esperado;
}

static void uso(const char *prog)
{
    printf("Uso: %s <caso> [threads]\n", prog);
    printf("  <caso>   = 1..%d | todos | lista\n", NUM_CASOS);
    printf("  [threads] = 1..%d (padrao: 2)\n", MAX_THREADS);
}

int main(int argc, char *argv[])
{
    int i, n, nthreads = 2, ok = 1;

    if (argc < 2 || argc > 3) {
        uso(argv[0]);
        return 1;
    }

    if (argc == 3) {
        nthreads = atoi(argv[2]);
        if (nthreads < 1 || nthreads > MAX_THREADS) {
            printf("Quantidade de threads invalida: %s\n", argv[2]);
            uso(argv[0]);
            return 1;
        }
    }

    if (strcmp(argv[1], "lista") == 0) {
        for (i = 0; i < NUM_CASOS; i++)
            printf("%d: %s (%dx%d, esperado %d)\n", i + 1, casos[i].nome,
                   casos[i].linhas, casos[i].colunas, casos[i].esperado);
        return 0;
    }

    if (strcmp(argv[1], "todos") == 0) {
        for (i = 1; i <= NUM_CASOS; i++)
            if (!executa_caso(i, nthreads)) ok = 0;
        return ok ? 0 : 1;
    }

    n = atoi(argv[1]);
    if (n < 1 || n > NUM_CASOS) {
        printf("Caso invalido: %s\n", argv[1]);
        uso(argv[0]);
        return 1;
    }

    return executa_caso(n, nthreads) ? 0 : 1;
}