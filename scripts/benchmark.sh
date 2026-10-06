#!/bin/sh
# Mede o tempo das versoes sequencial e paralela sobre a mesma matriz e grava
# os dados brutos em CSV. Cada execucao e um processo novo; o tempo medido e
# o impresso pelo proprio programa (tempo_ms, so a contagem, sem a leitura
# do arquivo).
#
# Uso: scripts/benchmark.sh [arquivo] [repeticoes] [threads...]
#   padrao: tests/adicionais/matriz_grande.txt, 10 repeticoes, 1 2 4 6 8 12
#
# Antes das repeticoes de cada configuracao ha 1 execucao de aquecimento,
# que e descartada (aquece o cache de disco e da CPU).

set -eu

ARQ=${1:-tests/adicionais/matriz_grande.txt}
REPS=${2:-10}
if [ $# -ge 3 ]; then shift 2; THREADS="$*"; else THREADS="1 2 4 6 8 12"; fi
CSV=${CSV:-results/medicoes.csv}
SEQ=build/sequencial
PAR=build/paralelo

[ -x "$SEQ" ] && [ -x "$PAR" ] || { echo "compile antes com: make" >&2; exit 1; }
mkdir -p "$(dirname "$CSV")"

NOME=$(basename "$ARQ" .txt)
echo "matriz,linhas,colunas,versao,trabalhadores,repeticao,tempo_ms,objetos,resultado_correto" > "$CSV"

# executa <versao> <trabalhadores> <comando...>: roda 1 aquecimento + REPS vezes
executa() {
    versao=$1; trab=$2; shift 2
    "$@" > /dev/null
    r=1
    while [ "$r" -le "$REPS" ]; do
        saida=$("$@") || { echo "falhou: $*" >&2; echo "$saida" >&2; exit 1; }
        dims=$(echo "$saida" | sed -n 's/.*(\([0-9]*\)x\([0-9]*\)).*/\1,\2/p')
        obj=$(echo "$saida" | sed -n 's/.*obtido=\([0-9]*\).*/\1/p')
        tempo=$(echo "$saida" | sed -n 's/.*tempo_ms=\([0-9.]*\).*/\1/p')
        case "$saida" in *FALHOU*) ok=false ;; *) ok=true ;; esac
        echo "$NOME,$dims,$versao,$trab,$r,$tempo,$obj,$ok" >> "$CSV"
        r=$((r + 1))
    done
    echo "  $versao com $trab trabalhador(es): $REPS repeticoes"
}

echo "Medindo $ARQ ($REPS repeticoes por configuracao)"
executa sequencial 1 "$SEQ" -f "$ARQ"
for t in $THREADS; do
    executa paralela "$t" "$PAR" -f "$ARQ" "$t"
done
echo "Dados brutos em $CSV"
