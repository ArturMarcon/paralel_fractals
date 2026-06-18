#ifndef COMMON_SORT_H
#define COMMON_SORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_REVERSE 0
#define INPUT_RANDOM 1
#define INPUT_ALMOST_SORTED 2

#if defined(__GNUC__) || defined(__clang__)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif

static void die_if_null(const void *ptr, const char *name)
{
    if (ptr == NULL) {
        fprintf(stderr, "Erro ao alocar memoria para %s\n", name);
        exit(EXIT_FAILURE);
    }
}

static void bs(int n, int *vetor)
{
    int c = 0;
    int trocou = 1;

    while (c < (n - 1) && trocou) {
        int d;
        trocou = 0;
        for (d = 0; d < n - c - 1; d++) {
            if (vetor[d] > vetor[d + 1]) {
                int troca = vetor[d];
                vetor[d] = vetor[d + 1];
                vetor[d + 1] = troca;
                trocou = 1;
            }
        }
        c++;
    }
}

static int *MAYBE_UNUSED interleaving(int vetor[], int tam)
{
    int *vetor_auxiliar;
    int i1 = 0;
    int i2 = tam / 2;
    int i_aux = 0;

    vetor_auxiliar = (int *)malloc(sizeof(int) * (size_t)tam);
    die_if_null(vetor_auxiliar, "vetor_auxiliar");

    for (i_aux = 0; i_aux < tam; i_aux++) {
        if ((i1 < (tam / 2)) && ((i2 == tam) || (vetor[i1] <= vetor[i2]))) {
            vetor_auxiliar[i_aux] = vetor[i1++];
        } else {
            vetor_auxiliar[i_aux] = vetor[i2++];
        }
    }

    return vetor_auxiliar;
}

static void init_vector(int *vetor, int tam, int input_mode)
{
    int i;

    if (input_mode == INPUT_RANDOM) {
        srand(314159);
        for (i = 0; i < tam; i++) {
            vetor[i] = rand() % tam;
        }
        return;
    }

    if (input_mode == INPUT_ALMOST_SORTED) {
        for (i = 0; i < tam; i++) {
            vetor[i] = i;
        }
        for (i = 0; i + 1 < tam; i += 1000) {
            int troca = vetor[i];
            vetor[i] = vetor[i + 1];
            vetor[i + 1] = troca;
        }
        return;
    }

    for (i = 0; i < tam; i++) {
        vetor[i] = tam - i;
    }
}

static int is_sorted(const int *vetor, int tam)
{
    int i;

    for (i = 1; i < tam; i++) {
        if (vetor[i - 1] > vetor[i]) {
            return 0;
        }
    }

    return 1;
}

static int parse_input_mode(const char *value)
{
    if (strcmp(value, "random") == 0) {
        return INPUT_RANDOM;
    }
    if (strcmp(value, "almost") == 0) {
        return INPUT_ALMOST_SORTED;
    }
    return INPUT_REVERSE;
}

#endif
