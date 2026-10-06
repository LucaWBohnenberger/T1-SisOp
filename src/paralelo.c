/*
 * Contagem PARALELA de objetos em matriz binaria (conectividade 8).
 * Versao com Pthreads e divisao da matriz em FAIXAS DE LINHAS.
 *
 * Estrategia (2 fases):
 *
 *   FASE 1 - PARALELA: a matriz e dividida em faixas de linhas. Cada thread
 *            cuida de uma faixa e faz flood fill SOMENTE dentro dela,
 *            rotulando as celulas. Cada objeto local recebe como rotulo o
 *            numero da sua celula inicial (linha * colunas + coluna + 1),
 *            que e unico na matriz inteira. Como cada thread escreve apenas
 *            nas suas linhas (e nas posicoes do union-find dos seus
 *            proprios rotulos), nao ha condicao de corrida.
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
 * O flood fill e ITERATIVO (cada thread tem sua propria pilha no heap), para
 * nao estourar a pilha de chamadas em objetos com milhoes de celulas.
 *
 * Compilar:
 *   cc -std=c89 -Wall -Wextra -pedantic -O2 -pthread src/paralelo.c -o build/paralelo
 */
#define _POSIX_C_SOURCE 199309L   /* necessario para clock_gettime em C89 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_THREADS 64
#define MAX_DIM 20000             /* limite de linhas e de colunas       */
#define MAX_CELULAS 100000000L    /* limite de linhas * colunas (10^8)   */

static unsigned char *matriz;     /* 0 = fundo, 1 = objeto                */
static int *rotulo;               /* 0 = sem rotulo, senao id do objeto   */
static int *pai;                  /* union-find sobre os rotulos (1..n)   */
static int linhas, colunas;

/* Acesso a celula (l, c) de um vetor linear linhas x colunas. */
#define CEL(v, l, c) ((v)[(l) * colunas + (c)])

/* Trabalho entregue a cada thread. */
struct tarefa {
    int ini;       /* primeira linha da faixa (inclusive) */
    int fim;       /* ultima linha da faixa (exclusive)   */
    int *pilha;    /* pilha privada do flood fill          */
    int objetos;   /* resultado: objetos locais da faixa  */
};

/* ---------------- Memoria da matriz ---------------- */

static void libera_matriz(void)
{
    free(matriz);
    free(rotulo);
    free(pai);
    matriz = NULL;
    rotulo = NULL;
    pai = NULL;
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
    rotulo = (int *)malloc(n * sizeof(int));
    pai = (int *)malloc((n + 1) * sizeof(int));   /* rotulos vao de 1 a n */
    if (matriz == NULL || rotulo == NULL || pai == NULL) {
        fprintf(stderr, "Memoria insuficiente para %dx%d\n", l, c);
        libera_matriz();
        return -1;
    }

    linhas = l;
    colunas = c;
    return 0;
}

/* ---------------- FASE 1: flood fill dentro da faixa ---------------- */

static void inunda(int l0, int c0, int id, int ini, int fim, int *pilha)
{
    int topo = 0, idx, l, c, dl, dc, nl, nc;

    /* a celula e rotulada ao ENTRAR na pilha, entao nunca entra duas vezes */
    CEL(rotulo, l0, c0) = id;
    pilha[topo++] = l0 * colunas + c0;

    while (topo > 0) {
        idx = pilha[--topo];
        l = idx / colunas;
        c = idx % colunas;

        for (dl = -1; dl <= 1; dl++)
            for (dc = -1; dc <= 1; dc++) {
                if (dl == 0 && dc == 0) continue;
                nl = l + dl;
                nc = c + dc;
                /* nao sai da faixa [ini, fim) nem da matriz */
                if (nl < ini || nl >= fim || nc < 0 || nc >= colunas) continue;
                if (CEL(matriz, nl, nc) == 0) continue;
                if (CEL(rotulo, nl, nc) != 0) continue;   /* ja rotulada */

                CEL(rotulo, nl, nc) = id;
                pilha[topo++] = nl * colunas + nc;
            }
    }
}

/* Funcao executada por cada thread. */
static void *trabalhador(void *arg)
{
    struct tarefa *t = (struct tarefa *)arg;
    int l, c, id;

    /* zera os rotulos da propria faixa (em paralelo, sem disputa) */
    memset(&CEL(rotulo, t->ini, 0), 0,
           (size_t)(t->fim - t->ini) * (size_t)colunas * sizeof(int));

    t->objetos = 0;
    for (l = t->ini; l < t->fim; l++)
        for (c = 0; c < colunas; c++)
            if (CEL(matriz, l, c) == 1 && CEL(rotulo, l, c) == 0) {
                t->objetos++;
                id = l * colunas + c + 1;
                pai[id] = id;     /* posicao exclusiva desta thread */
                inunda(l, c, id, t->ini, t->fim, t->pilha);
            }

    return NULL;
}

/* ---------------- FASE 2: union-find ---------------- */

/* Acha o representante de x, encurtando o caminho (path halving). */
static int acha(int x)
{
    while (pai[x] != x) {
        pai[x] = pai[pai[x]];
        x = pai[x];
    }
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
    int base, resto, ini, i, l, c, dc, c2, total, erro, criadas;

    if (nthreads > linhas) nthreads = linhas;   /* no minimo 1 linha/thread */

    /* divide as linhas em faixas (as primeiras faixas levam o resto) */
    base = linhas / nthreads;
    resto = linhas % nthreads;
    ini = 0;
    for (i = 0; i < nthreads; i++) {
        tarefas[i].ini = ini;
        tarefas[i].fim = ini + base + (i < resto ? 1 : 0);
        ini = tarefas[i].fim;
        /* cada celula da faixa entra no maximo uma vez na pilha */
        tarefas[i].pilha = (int *)malloc((size_t)(tarefas[i].fim - tarefas[i].ini)
                                         * (size_t)colunas * sizeof(int));
        if (tarefas[i].pilha == NULL) {
            fprintf(stderr, "Memoria insuficiente para as pilhas\n");
            while (i > 0) free(tarefas[--i].pilha);
            return -1;
        }
    }

    /* FASE 1: cria as threads */
    erro = 0;
    for (criadas = 0; criadas < nthreads; criadas++) {
        erro = pthread_create(&tid[criadas], NULL, trabalhador, &tarefas[criadas]);
        if (erro != 0) {
            fprintf(stderr, "pthread_create falhou: %s\n", strerror(erro));
            break;
        }
    }

    /* espera todas as que foram criadas terminarem (mesmo em caso de erro) */
    for (i = 0; i < criadas; i++) {
        if (pthread_join(tid[i], NULL) != 0) {
            fprintf(stderr, "pthread_join falhou\n");
            erro = 1;
        }
    }

    for (i = 0; i < nthreads; i++) free(tarefas[i].pilha);
    if (erro != 0) return -1;

    /* FASE 2: soma local e correcao nas fronteiras */
    total = 0;
    for (i = 0; i < nthreads; i++) total += tarefas[i].objetos;

    for (i = 0; i < nthreads - 1; i++) {
        l = tarefas[i].fim - 1;               /* ultima linha da faixa i  */
        for (c = 0; c < colunas; c++) {
            if (CEL(rotulo, l, c) == 0) continue;
            /* vizinhos da linha de baixo: esquerda, centro, direita */
            for (dc = -1; dc <= 1; dc++) {
                c2 = c + dc;
                if (c2 < 0 || c2 >= colunas) continue;
                if (CEL(rotulo, l + 1, c2) != 0)
                    if (uniao(CEL(rotulo, l, c), CEL(rotulo, l + 1, c2)))
                        total--;              /* eram o mesmo objeto */
            }
        }
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

    if (carrega(t->dados, t->linhas, t->colunas) != 0) return 0;
    obtido = conta_objetos(nthreads);

    printf("Ex%d - %s (%dx%d) [%d threads]: obtido=%d esperado=%d [%s]\n",
           n, t->nome, t->linhas, t->colunas, nthreads, obtido, t->esperado,
           obtido == t->esperado ? "OK" : "FALHOU");

    return obtido == t->esperado;
}

/*
 * Executa a matriz de um arquivo. So a contagem (criacao das threads,
 * fase 1, joins e fase 2) e cronometrada; a leitura do arquivo fica de
 * fora. Retorna 1 se passou, 0 se falhou.
 */
static int executa_arquivo(const char *caminho, int nthreads)
{
    int obtido, esperado;
    double ini, fim;

    if (le_arquivo(caminho, &esperado) != 0) return 0;

    ini = agora_ms();
    obtido = conta_objetos(nthreads);
    fim = agora_ms();
    if (obtido < 0) return 0;

    printf("%s (%dx%d) [%d threads]: obtido=%d", caminho, linhas, colunas,
           nthreads, obtido);
    if (esperado >= 0)
        printf(" esperado=%d [%s]", esperado,
               obtido == esperado ? "OK" : "FALHOU");
    printf(" tempo_ms=%.3f\n", fim - ini);

    return esperado < 0 || obtido == esperado;
}

static void uso(const char *prog)
{
    printf("Uso: %s <caso> [threads]\n", prog);
    printf("     %s -f <arquivo.txt> [threads]\n", prog);
    printf("  <caso>   = 1..%d | todos | lista\n", NUM_CASOS);
    printf("  -f <arquivo> le a matriz de um arquivo texto\n");
    printf("  [threads] = 1..%d (padrao: 2)\n", MAX_THREADS);
}

/* Le o numero de threads de s. Retorna o valor ou -1 se invalido. */
static int le_threads(const char *s)
{
    int n = atoi(s);
    if (n < 1 || n > MAX_THREADS) {
        printf("Quantidade de threads invalida: %s\n", s);
        return -1;
    }
    return n;
}

int main(int argc, char *argv[])
{
    int i, n, nthreads = 2, ok = 1;

    if (argc >= 3 && strcmp(argv[1], "-f") == 0) {
        if (argc > 4) {
            uso(argv[0]);
            return 1;
        }
        if (argc == 4 && (nthreads = le_threads(argv[3])) < 0) {
            uso(argv[0]);
            return 1;
        }
        ok = executa_arquivo(argv[2], nthreads);
        libera_matriz();
        return ok ? 0 : 1;
    }

    if (argc < 2 || argc > 3) {
        uso(argv[0]);
        return 1;
    }

    if (argc == 3 && (nthreads = le_threads(argv[2])) < 0) {
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
            if (!executa_caso(i, nthreads)) ok = 0;
        libera_matriz();
        return ok ? 0 : 1;
    }

    n = atoi(argv[1]);
    if (n < 1 || n > NUM_CASOS) {
        printf("Caso invalido: %s\n", argv[1]);
        uso(argv[0]);
        return 1;
    }

    ok = executa_caso(n, nthreads);
    libera_matriz();
    return ok ? 0 : 1;
}
