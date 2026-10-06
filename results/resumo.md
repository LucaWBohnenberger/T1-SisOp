Matriz `matriz_grande` (2000x2000, 8 objetos), 10 repeticoes por configuracao, valor representativo = mediana, dispersao = intervalo interquartil (Q1-Q3).

| Versao | Trabalhadores (p) | Mediana (ms) | Q1-Q3 (ms) | Min-Max (ms) | Aceleracao S(p) | Eficiencia E(p) | Resultado correto? |
|---|---:|---:|---:|---:|---:|---:|---|
| Sequencial | 1 | 52.52 | 52.16-53.07 | 35.62-53.85 | 1.00 | 1.00 | Sim |
| Paralela | 1 | 85.23 | 84.51-85.55 | 83.50-90.21 | 0.62 | 0.62 | Sim |
| Paralela | 2 | 49.94 | 48.60-51.30 | 47.65-57.16 | 1.05 | 0.53 | Sim |
| Paralela | 4 | 37.68 | 37.27-38.97 | 35.86-51.34 | 1.39 | 0.35 | Sim |
| Paralela | 6 | 41.11 | 32.84-42.30 | 29.78-43.36 | 1.28 | 0.21 | Sim |
| Paralela | 8 | 31.51 | 30.81-31.88 | 28.05-35.11 | 1.67 | 0.21 | Sim |
| Paralela | 12 | 21.21 | 19.88-23.08 | 19.46-25.23 | 2.48 | 0.21 | Sim |
