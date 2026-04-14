#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 3840
#define HEIGHT 2160
#define MAX_ITER 10000

const double C_REAL = -0.8;
const double C_IMAG = 0.156;

int compute_julia_pixel(int x, int y) {

  double z_real = 0.5 * (x - WIDTH / 2.0) / (0.5 * WIDTH);
  double z_imag = 0.5 * (y - HEIGHT / 2.0) / (0.5 * HEIGHT);

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

int main() {
  int *image = (int *)malloc(WIDTH * HEIGHT * sizeof(int));
  if (image == NULL) {
    printf("Erro fatal: falha ao alocar memoria para a imagem.\n");
    return 1;
  }

  printf("Iniciando calculo do Conjunto de Julia...\n");
  printf("Resolucao: %dx%d\n", WIDTH, HEIGHT);
  printf("Limite de iteracoes: %d\n", MAX_ITER);
  printf("Constante C: %f + %fi\n", C_REAL, C_IMAG);

  double start_time = omp_get_wtime();

/* *
 * Estatico: #pragma omp parallel for schedule(static)
 * Dinamico: #pragma omp parallel for schedule(dynamic, 64)
 */
#pragma omp parallel for schedule(static)
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int iter = compute_julia_pixel(x, y);
      image[y * WIDTH + x] = iter;
    }
  }

  double end_time = omp_get_wtime();
  double total_time = end_time - start_time;

  printf("-> Tempo total de execucao: %f segundos\n", total_time);

  printf("Salvando a imagem no disco...\n");
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
    printf("Concluido! Imagem salva como 'fractal_julia.pgm'.\n");
  } else {
    printf("Erro ao criar o arquivo de imagem.\n");
  }

  free(image);
  return 0;
}