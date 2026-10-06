#!/usr/bin/env python3
"""
Gera tests/adicionais/matriz_grande.txt: matriz 2000 x 2000 com 8 objetos, usada nos testes de desempenho.

Script auxiliar (nao faz parte da solucao em C). Depois de gerar, conta os
objetos com uma BFS independente em Python para conferir que sao 8.

Os objetos foram escolhidos para estressar a versao paralela: quase todos
atravessam varias faixas de linhas, e alguns so se conectam pela diagonal.

  1. Retangulo cheio                (linhas   10-400,  colunas   10-600)
  2. Moldura quadrada (anel)        (linhas   10-600,  colunas  700-1300)
  3. Tabuleiro de xadrez            (linhas   10-600,  colunas 1400-1990)
     -> celulas ligadas SO pela diagonal; e um unico objeto
  4. Linha diagonal de 1 celula     (linhas  650-1240, colunas   10-600)
  5. Serpentina (zigue-zague)       (linhas  700-1300, colunas  700-1300)
     -> caminho muito longo, testa o flood fill iterativo
  6. Letra "X" (duas diagonais)     (linhas  700-1300, colunas 1400-1990)
  7. Pente (espinha + dentes)       (linhas 1400-1990, colunas   10-600)
  8. Disco cheio de raio 300        (centro (1650, 1650))

Uso: python3 scripts/gera_matriz_grande.py [saida]
"""
import sys
from collections import deque

N = 2000
ESPERADO = 8


def gera():
    m = [bytearray(N) for _ in range(N)]

    def ret(l0, l1, c0, c1):
        for l in range(l0, l1 + 1):
            for c in range(c0, c1 + 1):
                m[l][c] = 1

    # 1. retangulo cheio
    ret(10, 400, 10, 600)

    # 2. moldura de espessura 5
    ret(10, 14, 700, 1300)
    ret(596, 600, 700, 1300)
    ret(10, 600, 700, 704)
    ret(10, 600, 1296, 1300)

    # 3. tabuleiro de xadrez (so diagonais)
    for l in range(10, 601):
        for c in range(1400, 1991):
            if (l + c) % 2 == 0:
                m[l][c] = 1

    # 4. diagonal de 1 celula de largura
    for k in range(0, 591):
        m[650 + k][10 + k] = 1

    # 5. serpentina: linhas horizontais a cada 4 linhas, ligadas
    #    alternadamente na ponta direita e na esquerda
    linhas_h = list(range(700, 1301, 4))
    for i, l in enumerate(linhas_h):
        ret(l, l, 700, 1300)
        if i + 1 < len(linhas_h):
            c = 1300 if i % 2 == 0 else 700
            ret(l, linhas_h[i + 1], c, c)

    # 6. letra X
    for k in range(0, 591):
        m[700 + k][1400 + k] = 1
        m[700 + k][1990 - k] = 1

    # 7. pente: espinha horizontal e dentes verticais a cada 6 colunas
    ret(1400, 1404, 10, 600)
    for c in range(10, 601, 6):
        ret(1405, 1990, c, c + 1)

    # 8. disco
    cl, cc, r = 1650, 1650, 300
    for l in range(cl - r, cl + r + 1):
        for c in range(cc - r, cc + r + 1):
            if (l - cl) ** 2 + (c - cc) ** 2 <= r * r:
                m[l][c] = 1

    return m


def conta(m):
    """BFS iterativa independente, conectividade 8."""
    vis = [bytearray(N) for _ in range(N)]
    total = 0
    for l in range(N):
        for c in range(N):
            if m[l][c] and not vis[l][c]:
                total += 1
                vis[l][c] = 1
                fila = deque([(l, c)])
                while fila:
                    a, b = fila.popleft()
                    for da in (-1, 0, 1):
                        for db in (-1, 0, 1):
                            x, y = a + da, b + db
                            if 0 <= x < N and 0 <= y < N and m[x][y] and not vis[x][y]:
                                vis[x][y] = 1
                                fila.append((x, y))
    return total


def main():
    saida = sys.argv[1] if len(sys.argv) > 1 else "tests/adicionais/matriz_grande.txt"
    m = gera()
    obtido = conta(m)
    if obtido != ESPERADO:
        sys.exit(f"erro: a matriz gerada tem {obtido} objetos, esperado {ESPERADO}")
    with open(saida, "w") as f:
        f.write("# Matriz grande 2000x2000 com 8 objetos (gerada por scripts/gera_matriz_grande.py)\n")
        f.write("# Valores sem separador: cada linha do arquivo e uma linha da matriz.\n")
        f.write(f"{N} {N} {ESPERADO}\n")
        for linha in m:
            f.write("".join("1" if v else "0" for v in linha))
            f.write("\n")
    print(f"{saida}: {N}x{N}, {obtido} objetos (conferido por BFS em Python)")


if __name__ == "__main__":
    main()
