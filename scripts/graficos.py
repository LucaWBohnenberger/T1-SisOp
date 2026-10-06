#!/usr/bin/env python3
"""
Le results/medicoes.csv (gerado por scripts/benchmark.sh), calcula a mediana
de cada configuracao, a aceleracao S(p) = Tseq / Tpar(p) e a eficiencia
E(p) = S(p) / p, e gera:

  results/resumo.md               tabela consolidada (Markdown)
  results/grafico-tempo.png       tempo por configuracao
  results/grafico-aceleracao.png  aceleracao x trabalhadores
  results/grafico-eficiencia.png  eficiencia x trabalhadores

Script auxiliar (nao faz parte da solucao em C). Requer matplotlib.
Uso: python3 scripts/graficos.py [results/medicoes.csv]
"""
import csv
import os
import statistics
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

AZUL = "#2a78d6"      # versao paralela
LARANJA = "#eb6834"   # versao sequencial
TEXTO = "#0b0b0b"
TEXTO2 = "#52514e"
GRADE = "#e4e3df"
FUNDO = "#fcfcfb"


def quartis(v):
    q = statistics.quantiles(v, n=4, method="inclusive")
    return q[0], q[2]


def le(caminho):
    grupos = {}
    with open(caminho) as f:
        for row in csv.DictReader(f):
            chave = (row["versao"], int(row["trabalhadores"]))
            g = grupos.setdefault(chave, {"t": [], "ok": True, "row": row})
            g["t"].append(float(row["tempo_ms"]))
            g["ok"] = g["ok"] and row["resultado_correto"] == "true"
    return grupos


def estilo(ax, titulo, xlabel, ylabel):
    ax.set_facecolor(FUNDO)
    ax.set_title(titulo, color=TEXTO, fontsize=12, loc="left", pad=12)
    ax.set_xlabel(xlabel, color=TEXTO2)
    ax.set_ylabel(ylabel, color=TEXTO2)
    ax.tick_params(colors=TEXTO2, length=0)
    ax.grid(axis="y", color=GRADE, linewidth=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right", "left"):
        ax.spines[s].set_visible(False)
    ax.spines["bottom"].set_color(GRADE)


def main():
    caminho = sys.argv[1] if len(sys.argv) > 1 else "results/medicoes.csv"
    saida = os.path.dirname(caminho) or "."
    grupos = le(caminho)

    seq = grupos[("sequencial", 1)]
    t_seq = statistics.median(seq["t"])
    par = sorted((p, g) for (v, p), g in grupos.items() if v == "paralela")
    exemplo = seq["row"]

    linhas = []
    for nome, p, g in [("Sequencial", 1, seq)] + [("Paralela", p, g) for p, g in par]:
        med = statistics.median(g["t"])
        q1, q3 = quartis(g["t"])
        s = t_seq / med
        e = s / p
        linhas.append((nome, p, med, q1, q3, min(g["t"]), max(g["t"]), s, e, g["ok"], len(g["t"])))

    with open(os.path.join(saida, "resumo.md"), "w") as f:
        f.write(f"Matriz `{exemplo['matriz']}` ({exemplo['linhas']}x{exemplo['colunas']}, "
                f"{exemplo['objetos']} objetos), {linhas[0][10]} repeticoes por configuracao, "
                "valor representativo = mediana, dispersao = intervalo interquartil (Q1-Q3).\n\n")
        f.write("| Versao | Trabalhadores (p) | Mediana (ms) | Q1-Q3 (ms) | Min-Max (ms) "
                "| Aceleracao S(p) | Eficiencia E(p) | Resultado correto? |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|---|\n")
        for nome, p, med, q1, q3, mn, mx, s, e, ok, _ in linhas:
            f.write(f"| {nome} | {p} | {med:.2f} | {q1:.2f}-{q3:.2f} | {mn:.2f}-{mx:.2f} "
                    f"| {s:.2f} | {e:.2f} | {'Sim' if ok else 'Nao'} |\n")
    print(open(os.path.join(saida, "resumo.md")).read())

    ps = [p for p, _ in par]
    meds = [statistics.median(g["t"]) for _, g in par]
    s_par = [t_seq / m for m in meds]

    # --- tempo ---
    fig, ax = plt.subplots(figsize=(8, 4.5), facecolor=FUNDO)
    rot = ["seq"] + [f"par {p}" for p in ps]
    vals = [t_seq] + meds
    erros_baixo, erros_cima = [], []
    for g in [seq] + [g for _, g in par]:
        med = statistics.median(g["t"])
        q1, q3 = quartis(g["t"])
        erros_baixo.append(med - q1)
        erros_cima.append(q3 - med)
    cores = [LARANJA] + [AZUL] * len(ps)
    barras = ax.bar(rot, vals, color=cores, width=0.6, edgecolor=FUNDO, linewidth=2,
                    yerr=[erros_baixo, erros_cima], ecolor=TEXTO2, capsize=3)
    for b, v in zip(barras, vals):
        ax.text(b.get_x() + b.get_width() / 2, v * 1.02 + max(vals) * 0.02, f"{v:.1f}",
                ha="center", va="bottom", fontsize=8, color=TEXTO2)
    estilo(ax, "Tempo de contagem (mediana) por configuracao",
           "versao e numero de threads", "tempo (ms)")
    ax.legend(handles=[plt.Rectangle((0, 0), 1, 1, color=LARANJA),
                       plt.Rectangle((0, 0), 1, 1, color=AZUL)],
              labels=["sequencial", "paralela (Pthreads)"], frameon=False,
              labelcolor=TEXTO2, loc="upper right")
    ax.set_ylim(0, max(vals) * 1.2)
    fig.tight_layout()
    fig.savefig(os.path.join(saida, "grafico-tempo.png"), dpi=150)
    plt.close(fig)

    # --- aceleracao ---
    fig, ax = plt.subplots(figsize=(8, 4.5), facecolor=FUNDO)
    ax.plot(ps, ps, linestyle="--", color=TEXTO2, linewidth=1.2, label="ideal S(p) = p")
    ax.plot(ps, s_par, color=AZUL, linewidth=2, marker="o", markersize=7,
            markeredgecolor=FUNDO, markeredgewidth=2, label="observada")
    for p, s in zip(ps, s_par):
        ax.annotate(f"{s:.2f}", (p, s), textcoords="offset points", xytext=(0, -16),
                    ha="center", fontsize=8, color=TEXTO2)
    ax.axhline(1, color=GRADE, linewidth=1)
    estilo(ax, "Aceleracao S(p) = Tsequencial / Tparalelo(p)", "threads (p)", "aceleracao")
    ax.set_xticks(ps)
    ax.set_ylim(0, max(ps) * 1.05)
    ax.legend(frameon=False, labelcolor=TEXTO2, loc="upper left")
    fig.tight_layout()
    fig.savefig(os.path.join(saida, "grafico-aceleracao.png"), dpi=150)
    plt.close(fig)

    # --- eficiencia ---
    fig, ax = plt.subplots(figsize=(8, 4.5), facecolor=FUNDO)
    efs = [s / p for s, p in zip(s_par, ps)]
    ax.axhline(1, linestyle="--", color=TEXTO2, linewidth=1.2, label="ideal E(p) = 1")
    ax.plot(ps, efs, color=AZUL, linewidth=2, marker="o", markersize=7,
            markeredgecolor=FUNDO, markeredgewidth=2, label="observada")
    for p, e in zip(ps, efs):
        ax.annotate(f"{e:.2f}", (p, e), textcoords="offset points", xytext=(0, 9),
                    ha="center", fontsize=8, color=TEXTO2)
    estilo(ax, "Eficiencia E(p) = S(p) / p", "threads (p)", "eficiencia")
    ax.set_xticks(ps)
    ax.set_ylim(0, 1.15)
    ax.legend(frameon=False, labelcolor=TEXTO2, loc="center right")
    fig.tight_layout()
    fig.savefig(os.path.join(saida, "grafico-eficiencia.png"), dpi=150)
    plt.close(fig)

    print(f"graficos gravados em {saida}/")


if __name__ == "__main__":
    main()
