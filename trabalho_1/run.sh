#!/bin/zsh

# Compilação no macOS (assumindo uso do Apple Clang com libomp instalada via Homebrew)
# Caso use o GCC do Homebrew, substitua por: gcc-14 -fopenmp julia.c -o julia
gcc -fopenmp julia.c -o julia

OUTPUT_FILE="resultados.csv"

# Cria o cabeçalho do CSV
echo "iteracao,threads,schedule,chunk_size,tempo_segundos" > $OUTPUT_FILE

REPETICOES=5
THREADS=(1 2 4 8)
SCHEDULES=("static" "dynamic" "guided")
CHUNKS=(1 32 64 128)

echo "Iniciando bateria de testes..."

for i in {1..$REPETICOES}; do
  for t in "${THREADS[@]}"; do
    for sched in "${SCHEDULES[@]}"; do
      for c in "${CHUNKS[@]}"; do
        
        # O resultado em stdout contém apenas as métricas CSV geradas pelo printf
        METRICAS=$(./julia $t $sched $c)
        
        # Grava a iteração atual seguida das métricas no arquivo de saída
        echo "$i,$METRICAS" >> $OUTPUT_FILE
        
      done
    done
  done
done

echo "Testes concluídos. Resultados salvos em $OUTPUT_FILE"