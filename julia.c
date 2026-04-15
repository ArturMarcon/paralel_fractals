#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 7680
#define HEIGHT 4320
#define MAX_ITER 10000

const double C_REAL = -0.8;
const double C_IMAG = 0.156;

int compute_julia_pixel(int x, int y) {

  double z_real = 0.1 * (x - WIDTH / 2.0) / (0.5 * WIDTH);
  double z_imag = 0.1 * (y - HEIGHT / 2.0) / (0.5 * HEIGHT);

  int iter;
  for (iter = 0; iter < MAX_ITER; iter++) {
    double z_real_sq = z_real * z_real;
    double z_imag_sq = z_imag * z_imag;

    if (z_real_sq + z_imag_sq > 4.0) {
      break;
    }

    double temp_real = z_real_sq - z_imag_sq + C_REAL;
    z_imag = 2.0 * z_real * z_imag + C_IMAG;
    z_real = temp_real;
  }
  return iter;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(
        stderr,
        "Uso: %s <num_threads> <tipo_schedule (static|dynamic)> <chunk_size>\n",
        argv[0]);
    return 1;
  }

  int num_threads = atoi(argv[1]);
  char *sched_type_str = argv[2];
  int chunk_size = atoi(argv[3]);

  // Configura o tipo de escalonamento
  omp_sched_t sched_type = omp_sched_static;
  if (strcmp(sched_type_str, "dynamic") == 0) {
    sched_type = omp_sched_dynamic;
  }

  // Define as configurações do OpenMP para a execução atual
  omp_set_num_threads(num_threads);
  omp_set_schedule(sched_type, chunk_size);

  int *image = (int *)malloc(WIDTH * HEIGHT * sizeof(int));
  if (image == NULL) {
    fprintf(stderr, "Erro fatal: falha ao alocar memoria para a imagem.\n");
    return 1;
  }

  fprintf(stderr, "Iniciando: %d threads | %s | chunk: %d\n", num_threads,
          sched_type_str, chunk_size);

  double start_time = omp_get_wtime();

// schedule(runtime) delega a decisão para as configurações definidas via
// omp_set_schedule
#pragma omp parallel for schedule(runtime)
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int iter = compute_julia_pixel(x, y);
      image[y * WIDTH + x] = iter;
    }
  }

  double end_time = omp_get_wtime();
  double total_time = end_time - start_time;

  // Imprime apenas os dados no stdout para facilitar a coleta no script
  printf("%d,%s,%d,%f\n", num_threads, sched_type_str, chunk_size, total_time);

  FILE *fp = fopen("fractal_julia.pgm", "wb");
  if (fp != NULL) {
    fprintf(fp, "P2\n%d %d\n%d\n", WIDTH, HEIGHT, MAX_ITER);
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
      fprintf(fp, "%d ", image[i]);
      if ((i + 1) % WIDTH == 0) {
        fprintf(fp, "\n");
      }
    }
    fclose(fp);
  }

  free(image);
  return 0;
}