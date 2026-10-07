Matriz `matriz_grande` (2000x2000, 8 objetos), 10 repeticoes por configuracao, valor representativo = mediana, dispersao = intervalo interquartil (Q1-Q3).

| Versao | Trabalhadores (p) | Mediana (ms) | Q1-Q3 (ms) | Min-Max (ms) | Aceleracao S(p) | Eficiencia E(p) | Resultado correto? |
|---|---:|---:|---:|---:|---:|---:|---|
| Sequencial | 1 | 5.45 | 5.42-5.54 | 5.18-5.90 | 1.00 | 1.00 | Sim |
| Paralela | 1 | 6.14 | 6.05-6.23 | 5.99-7.33 | 0.89 | 0.89 | Sim |
| Paralela | 2 | 3.50 | 3.49-3.56 | 3.48-4.02 | 1.56 | 0.78 | Sim |
| Paralela | 4 | 3.00 | 2.86-3.02 | 2.74-3.16 | 1.82 | 0.45 | Sim |
| Paralela | 6 | 2.60 | 2.54-2.68 | 2.42-2.91 | 2.09 | 0.35 | Sim |
| Paralela | 8 | 2.55 | 2.53-2.61 | 2.39-2.78 | 2.14 | 0.27 | Sim |
| Paralela | 12 | 2.56 | 2.53-2.66 | 2.25-2.74 | 2.13 | 0.18 | Sim |
