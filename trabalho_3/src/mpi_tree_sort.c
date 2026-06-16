#include <float.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include "common_sort.h"

#define TAG_WORK 10
#define TAG_STOP 11

static double tempo_ocupado_local = 0.0;
static int segmentos_conquistados_local = 0;

static void sort_fixed_tree(int rank, int nprocs, int *vetor, int tam, int delta)
{
    int left = 2 * rank + 1;
    int right = 2 * rank + 2;
    int meio = tam / 2;
    int *aux;
    double t_compute;
    MPI_Status status;

    /* Conquista: vetor pequeno (<= delta) ou sem filhos. Cronometra so o calculo. */
    if (tam <= delta || left >= nprocs || right >= nprocs || tam < 2) {
        t_compute = MPI_Wtime();
        bubble_sort(tam, vetor);
        tempo_ocupado_local += MPI_Wtime() - t_compute;
        segmentos_conquistados_local++;
        return;
    }

    /* Divisao: envia as metades aos filhos. A espera no MPI_Recv nao e cronometrada. */
    MPI_Send(vetor, meio, MPI_INT, left, TAG_WORK, MPI_COMM_WORLD);
    MPI_Send(vetor + meio, tam - meio, MPI_INT, right, TAG_WORK, MPI_COMM_WORLD);

    MPI_Recv(vetor, meio, MPI_INT, left, TAG_WORK, MPI_COMM_WORLD, &status);
    MPI_Recv(vetor + meio, tam - meio, MPI_INT, right, TAG_WORK, MPI_COMM_WORLD, &status);

    /* Intercalacao tambem e carga real: cronometrada. */
    aux = (int *)malloc((size_t)tam * sizeof(int));
    die_if_null(aux, "auxiliar");
    t_compute = MPI_Wtime();
    merge_sorted_halves(vetor, tam, aux);
    tempo_ocupado_local += MPI_Wtime() - t_compute;
    free(aux);
}

static void run_helper_fixed(int rank, int nprocs, int delta)
{
    MPI_Status status;
    int tam;
    int parent;
    int *vetor;

    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    if (status.MPI_TAG == TAG_STOP) {
        MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, TAG_STOP, MPI_COMM_WORLD, &status);
        return;
    }

    parent = status.MPI_SOURCE;
    MPI_Get_count(&status, MPI_INT, &tam);

    vetor = (int *)malloc((size_t)tam * sizeof(int));
    die_if_null(vetor, "vetor");

    /* O calculo ja e cronometrado dentro de sort_fixed_tree. */
    MPI_Recv(vetor, tam, MPI_INT, parent, TAG_WORK, MPI_COMM_WORLD, &status);
    sort_fixed_tree(rank, nprocs, vetor, tam, delta);
    MPI_Send(vetor, tam, MPI_INT, parent, TAG_WORK, MPI_COMM_WORLD);

    MPI_Recv(NULL, 0, MPI_INT, 0, TAG_STOP, MPI_COMM_WORLD, &status);
    free(vetor);
}

int main(int argc, char **argv)
{
    int rank;
    int nprocs;
    int tam = 10000;
    int delta = 1000;
    int input_mode = INPUT_REVERSE;
    int *vetor = NULL;
    double inicio = 0.0;
    double fim = 0.0;
    double tempo_minimo = 0.0;
    double tempo_maximo = 0.0;
    double tempo_soma = 0.0;
    double tempo_para_min;
    int ativo_local;
    int ativos_soma = 0;
    int segmentos_soma = 0;
    int i;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

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
        sort_fixed_tree(rank, nprocs, vetor, tam, delta);
        fim = MPI_Wtime();

        /* Sinaliza o fim do trabalho aos demais processos. */
        for (i = 1; i < nprocs; i++) {
            MPI_Send(NULL, 0, MPI_INT, i, TAG_STOP, MPI_COMM_WORLD);
        }
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        run_helper_fixed(rank, nprocs, delta);
    }

    /* Balanceamento: processos ociosos entram no MIN com DBL_MAX para nao zerar
       o minimo; desbalanceamento e media consideram so os ativos. */
    ativo_local = (tempo_ocupado_local > 0.0) ? 1 : 0;
    tempo_para_min = ativo_local ? tempo_ocupado_local : DBL_MAX;

    MPI_Reduce(&tempo_para_min, &tempo_minimo, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tempo_ocupado_local, &tempo_maximo, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tempo_ocupado_local, &tempo_soma, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&segmentos_conquistados_local, &segmentos_soma, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&ativo_local, &ativos_soma, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("programa=mpi_arvore tamanho=%d delta=%d processos=%d entrada=%d tempo=%.6f ordenado=%d tempo_min=%.6f tempo_max=%.6f tempo_medio=%.6f desbalanceamento=%.6f segmentos=%d processos_ativos=%d\n",
               tam, delta, nprocs, input_mode, fim - inicio, is_sorted(vetor, tam),
               tempo_minimo, tempo_maximo,
               ativos_soma > 0 ? tempo_soma / ativos_soma : 0.0,
               tempo_maximo > 0.0 ? (tempo_maximo - tempo_minimo) / tempo_maximo : 0.0,
               segmentos_soma, ativos_soma);
        free(vetor);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
