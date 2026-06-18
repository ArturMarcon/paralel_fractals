#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common_sort.h"

static double get_time(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

int main(int argc, char **argv)
{
    int tam = 10000;
    int input_mode = INPUT_REVERSE;
    int *vetor;
    double inicio;
    double fim;

    if (argc >= 2) {
        tam = atoi(argv[1]);
    }
    if (argc >= 3) {
        input_mode = parse_input_mode(argv[2]);
    }
    if (tam <= 0) {
        fprintf(stderr, "Uso: %s [tamanho] [reverse|random|almost]\n", argv[0]);
        return EXIT_FAILURE;
    }

    vetor = (int *)malloc((size_t)tam * sizeof(int));
    die_if_null(vetor, "vetor");

    init_vector(vetor, tam, input_mode);

    inicio = get_time();
    bs(tam, vetor);
    fim = get_time();

    printf("programa=sequencial tamanho=%d entrada=%d tempo=%.6f ordenado=%d\n",
           tam, input_mode, fim - inicio, is_sorted(vetor, tam));

    free(vetor);
    return EXIT_SUCCESS;
}
