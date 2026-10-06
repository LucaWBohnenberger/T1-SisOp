# Contagem de objetos em matriz binaria - versoes sequencial e paralela.
#
#   make                 compila as duas versoes em build/
#   make testes          roda todos os testes (casos embutidos + arquivos .txt)
#   make sequencial-run  roda os 5 casos obrigatorios na versao sequencial
#   make paralelo-run    roda os 5 casos obrigatorios na versao paralela
#   make grande          roda a matriz grande (2000x2000, 8 objetos) nas duas versoes
#   make arquivo ARQ=x   roda um arquivo .txt qualquer nas duas versoes
#   make bench           mede desempenho e gera results/medicoes.csv
#   make graficos        gera results/resumo.md e os graficos .png
#   make matriz-grande   regera tests/adicionais/matriz_grande.txt
#   make clean           apaga build/
#
# Variaveis: THREADS (padrao 4), ARQ, REPS (padrao 10), LISTA_THREADS.

CC      = cc
CFLAGS  = -std=c89 -Wall -Wextra -pedantic -O2
BUILD   = build

THREADS       ?= 4
ARQ           ?= tests/adicionais/matriz_grande.txt
REPS          ?= 10
LISTA_THREADS ?= 1 2 4 6 8 12
GRANDE         = tests/adicionais/matriz_grande.txt

SEQ = $(BUILD)/sequencial
PAR = $(BUILD)/paralelo

.PHONY: all testes sequencial-run paralelo-run grande arquivo bench graficos \
        matriz-grande clean

all: $(SEQ) $(PAR)

$(BUILD):
	mkdir -p $(BUILD)

$(SEQ): src/sequencial.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(PAR): src/paralelo.c | $(BUILD)
	$(CC) $(CFLAGS) -pthread $< -o $@

sequencial-run: $(SEQ)
	./$(SEQ) todos

paralelo-run: $(PAR)
	./$(PAR) todos $(THREADS)

# Casos embutidos com varias quantidades de threads e todos os .txt de tests/.
# Para no primeiro resultado diferente do esperado.
testes: all
	@echo "== Casos embutidos (sequencial)"
	@./$(SEQ) todos
	@for t in 1 2 3 4 8; do \
	    echo "== Casos embutidos (paralelo, $$t threads)"; \
	    ./$(PAR) todos $$t || exit 1; \
	done
	@echo "== Arquivos de tests/ (sequencial e paralelo com 1, 2, 3, 4 e 8 threads)"
	@for f in tests/obrigatorios/*.txt tests/adicionais/*.txt; do \
	    ./$(SEQ) -f $$f || exit 1; \
	    for t in 1 2 3 4 8; do ./$(PAR) -f $$f $$t > /dev/null || \
	        { ./$(PAR) -f $$f $$t; exit 1; }; done; \
	done
	@echo "== Todos os testes passaram"

grande: all
	./$(SEQ) -f $(GRANDE)
	./$(PAR) -f $(GRANDE) $(THREADS)

arquivo: all
	./$(SEQ) -f $(ARQ)
	./$(PAR) -f $(ARQ) $(THREADS)

bench: all
	sh scripts/benchmark.sh $(ARQ) $(REPS) $(LISTA_THREADS)

graficos:
	python3 scripts/graficos.py results/medicoes.csv

matriz-grande:
	python3 scripts/gera_matriz_grande.py $(GRANDE)

clean:
	rm -rf $(BUILD)
