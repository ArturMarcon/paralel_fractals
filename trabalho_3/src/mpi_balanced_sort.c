#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include "common_sort.h"

#define TAG_WORK 20
#define TAG_STOP 21

static double tempo_ocupado_local = 0.0;
static int segmentos_conquistados_local = 0;

static int pow2_int(int level)
{
    return 1 << level;
}

static int topmost_level(int rank)
{
    int level = 0;

    while (pow2_int(level) <= rank) {
        level++;
    }

    return level;
}

static void sort_balanced_tree(int rank, int max_rank, int *vetor, int tam, int delta, int level)
{
    int helper_rank = rank + pow2_int(level);
    int meio = tam / 2;
    int *aux;
    MPI_Status status;

    if (tam <= delta || helper_rank > max_rank || tam < 2) {
        bubble_sort(tam, vetor);
        segmentos_conquistados_local++;
        return;
    }

    MPI_Send(vetor + meio, tam - meio, MPI_INT, helper_rank, TAG_WORK, MPI_COMM_WORLD);

    sort_balanced_tree(rank, max_rank, vetor, meio, delta, level + 1);

    MPI_Recv(vetor + meio, tam - meio, MPI_INT, helper_rank, TAG_WORK,
             MPI_COMM_WORLD, &status);

    aux = (int *)malloc((size_t)tam * sizeof(int));
    die_if_null(aux, "auxiliar");
    merge_sorted_halves(vetor, tam, aux);
    free(aux);
}

static void run_helper_balanced(int rank, int max_rank, int delta)
{
    MPI_Status status;
    int tam;
    int parent;
    int *vetor;
    double inicio;

    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    if (status.MPI_TAG == TAG_STOP) {
        MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, TAG_STOP, MPI_COMM_WORLD, &status);
        return;
    }

    parent = status.MPI_SOURCE;
    MPI_Get_count(&status, MPI_INT, &tam);

    vetor = (int *)malloc((size_t)tam * sizeof(int));
    die_if_null(vetor, "vetor");

    MPI_Recv(vetor, tam, MPI_INT, parent, TAG_WORK, MPI_COMM_WORLD, &status);
    inicio = MPI_Wtime();
    sort_balanced_tree(rank, max_rank, vetor, tam, delta, topmost_level(rank));
    tempo_ocupado_local += MPI_Wtime() - inicio;
    MPI_Send(vetor, tam, MPI_INT, parent, TAG_WORK, MPI_COMM_WORLD);

    MPI_Recv(NULL, 0, MPI_INT, 0, TAG_STOP, MPI_COMM_WORLD, &status);
    free(vetor);
}

int main(int argc, char **argv)
{
    int rank;
    int nprocs;
    int max_rank;
    int tam = 10000;
    int delta = 1000;
    int input_mode = INPUT_REVERSE;
    int *vetor = NULL;
    double inicio = 0.0;
    double fim = 0.0;
    double tempo_minimo = 0.0;
    double tempo_maximo = 0.0;
    double tempo_soma = 0.0;
    int segmentos_soma = 0;
    int i;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    max_rank = nprocs - 1;

    if (argc >= 2) {
        tam = atoi(argv[1]);
    }
    if (argc >= 3) {
        delta = atoi(argv[2]);
    }
    if (argc >= 4) {
        input_mode = parse_input_mode(argv[3]);
    }
    if (tam <= 0 || delta <= 0) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s [tamanho] [delta] [reverse|random|almost]\n", argv[0]);
        }
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    if (rank == 0) {
        vetor = (int *)malloc((size_t)tam * sizeof(int));
        die_if_null(vetor, "vetor");
        init_vector(vetor, tam, input_mode);

        MPI_Barrier(MPI_COMM_WORLD);
        inicio = MPI_Wtime();
        sort_balanced_tree(rank, max_rank, vetor, tam, delta, 0);
        fim = MPI_Wtime();
        tempo_ocupado_local = fim - inicio;

        for (i = 1; i < nprocs; i++) {
            MPI_Send(NULL, 0, MPI_INT, i, TAG_STOP, MPI_COMM_WORLD);
        }

        MPI_Reduce(&tempo_ocupado_local, &tempo_minimo, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
        MPI_Reduce(&tempo_ocupado_local, &tempo_maximo, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&tempo_ocupado_local, &tempo_soma, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&segmentos_conquistados_local, &segmentos_soma, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

        printf("programa=mpi_balanceado tamanho=%d delta=%d processos=%d entrada=%d tempo=%.6f ordenado=%d tempo_min=%.6f tempo_max=%.6f tempo_medio=%.6f desbalanceamento=%.6f segmentos=%d\n",
               tam, delta, nprocs, input_mode, fim - inicio, is_sorted(vetor, tam),
               tempo_minimo, tempo_maximo, tempo_soma / nprocs,
               tempo_maximo > 0.0 ? (tempo_maximo - tempo_minimo) / tempo_maximo : 0.0,
               segmentos_soma);
        free(vetor);
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        run_helper_balanced(rank, max_rank, delta);

        MPI_Reduce(&tempo_ocupado_local, NULL, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
        MPI_Reduce(&tempo_ocupado_local, NULL, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&tempo_ocupado_local, NULL, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&segmentos_conquistados_local, NULL, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
