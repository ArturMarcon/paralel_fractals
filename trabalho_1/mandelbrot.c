#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

// Configuracoes da imagem (Resolucao 4K padrao)
#define WIDTH 3840
#define HEIGHT 2160
#define MAX_ITER 10000

// Funcao que calcula o numero de iteracoes para o conjunto de Mandelbrot
int compute_mandelbrot_pixel(int x, int y) {
  // Ajuste de escala para manter a proporcao da imagem correta (Aspect Ratio)
  // O fator 2.5 e o deslocamento de -0.5 centralizam o fractal na tela
  double scale = 2.5 / HEIGHT;
  double c_real = (x - WIDTH / 2.0) * scale - 0.5;
  double c_imag = (y - HEIGHT / 2.0) * scale;

  // No Mandelbrot, z sempre comeca em 0
  double z_real = 0.0;
  double z_imag = 0.0;

  int iter;
  for (iter = 0; iter < MAX_ITER; iter++) {
    double z_real_sq = z_real * z_real;
    double z_imag_sq = z_imag * z_imag;

    // Condicao de escape
    if (z_real_sq + z_imag_sq > 4.0) {
      break;
    }

    // Aplicacao da formula: z_{n+1} = z_n^2 + c
    z_imag = 2.0 * z_real * z_imag + c_imag;
    z_real = z_real_sq - z_imag_sq + c_real;
  }
  return iter;
}

int main() {
  // Alocacao dinamica
  int *image = (int *)malloc(WIDTH * HEIGHT * sizeof(int));
  if (image == NULL) {
    printf("Erro fatal: falha ao alocar memoria para a imagem.\n");
    return 1;
  }

  printf("Iniciando calculo do Conjunto de Mandelbrot...\n");
  printf("Resolucao: %dx%d\n", WIDTH, HEIGHT);
  printf("Limite de iteracoes: %d\n", MAX_ITER);

  double start_time = omp_get_wtime();

/* AREA DE INSTRUMENTACAO OPENMP
 * Alterne entre:
 * schedule(static)  -> Vai rodar muito mal no Mandelbrot (bom para o relatorio)
 * schedule(dynamic, 64) -> Vai balancear a carga entre os P-cores e E-cores do
 * seu Dell
 */
#pragma omp parallel for schedule(dynamic, 64)
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int iter = compute_mandelbrot_pixel(x, y);
      image[y * WIDTH + x] = iter;
    }
  }

  double end_time = omp_get_wtime();
  double total_time = end_time - start_time;

  printf("-> Tempo total de execucao: %f segundos\n", total_time);

  // Gravacao no disco
  printf("Salvando a imagem no disco...\n");
  FILE *fp = fopen("mandelbrot.pgm", "wb");
  if (fp != NULL) {
    fprintf(fp, "P2\n%d %d\n%d\n", WIDTH, HEIGHT, MAX_ITER);
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
      fprintf(fp, "%d ", image[i]);
      if ((i + 1) % WIDTH == 0) {
        fprintf(fp, "\n");
      }
    }
    fclose(fp);
    printf("Concluido! Imagem salva como 'mandelbrot.pgm'.\n");
  } else {
    printf("Erro ao criar o arquivo de imagem.\n");
  }

  free(image);
  return 0;
}