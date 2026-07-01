# Relatório - Trabalho 4: Versão Híbrida MPI + OpenMP

## 1. Análise de Desempenho e Escalabilidade

### 1.1 Tabelas de Resultados e Gráficos

*(Espaço reservado para as tabelas de dados de Strong e Weak Scaling, contendo tempo de execução, speedup, eficiência e desbalanceamento de carga, extraídas do `analyze.py`)*

*(Inserir Gráficos gerados na pasta `plots` aqui)*

### 1.2 Cálculo do Speed-up e Eficiência

O speedup $S(p)$ e a eficiência $E(p)$ foram calculados tomando como base de comparação o tempo de execução da versão estritamente sequencial (`julia_dive_seq`) rodando a mesma carga (128 frames) com o número total de *workers* de processamento correspondente $P$.

A matemática aplicada demonstra que:
- **Speedup $S(p) = \frac{T_{seq}}{T(p)}$**
- **Eficiência $E(p) = \frac{S(p)}{P}$**

Observa-se através das tabelas que a arquitetura Master/Worker ... *(completar com análise dos dados reais).*

### 1.3 Análise da Alocação do Coordenador

Comparamos a abordagem de dedicar um núcleo exclusivo para a thread/processo coordenador (deixando os workers com $T-1$ threads OpenMP naquele nó) vs a abordagem de *oversubscription* (onde o processo coordenador disputa fatias de tempo de CPU com os workers no mesmo nó, mantendo os workers usando todas as threads possíveis).

*(Completar com a análise de qual das duas se saiu melhor nas diferentes escalas de 1 a 64 workers baseado nos gráficos)*

### 1.4 Comparação: MPI Puro vs Híbrido

Testamos a versão em MPI puro escalando até 64 workers (1 processo por core) e a comparamos com o modelo híbrido que cria 1 processo pesado MPI por nó de processamento (tendo até 16 threads OpenMP leves operando através do modelo de Workpool).

*(Completar com a análise e conclusão de qual foi mais escalável)*

---
## 2. Código Fonte Principal (Workpool)

```c
// Trecho do julia_dive_hybrid.c
void compute_julia_frame(...) {
    // ... setup ...
    // WORKPOOL EM OPENMP (Sem mestre, threads trabalham nas iterações)
    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            // ... Computação ...
        }
    }
}
```
