#include <float.h>
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

static void sort_balanced_tree(int rank, int max_rank, int *vetor, int tam, int level)
{
    int helper_rank = rank + pow2_int(level);
    int meio = tam / 2;
    int *vetor_auxiliar;
    double t_compute;
    MPI_Status status;

    /* Conquista quando nao existe helper disponivel para continuar dividindo. */
    if (helper_rank > max_rank || tam < 2) {
        t_compute = MPI_Wtime();
        bs(tam, vetor);
        tempo_ocupado_local += MPI_Wtime() - t_compute;
        segmentos_conquistados_local++;
        return;
    }

    /* Balanceamento: delega so a metade superior ao helper (rank + 2^level) e
       ordena a inferior por conta propria, fazendo todo no participar da ordenacao. */
    MPI_Send(vetor + meio, tam - meio, MPI_INT, helper_rank, TAG_WORK, MPI_COMM_WORLD);

    sort_balanced_tree(rank, max_rank, vetor, meio, level + 1);

    MPI_Recv(vetor + meio, tam - meio, MPI_INT, helper_rank, TAG_WORK,
             MPI_COMM_WORLD, &status);

    /* Intercalacao tambem e carga real: cronometrada. */
    t_compute = MPI_Wtime();
    vetor_auxiliar = interleaving(vetor, tam);
    memcpy(vetor, vetor_auxiliar, (size_t)tam * sizeof(int));
    tempo_ocupado_local += MPI_Wtime() - t_compute;
    free(vetor_auxiliar);
}

static void run_helper_balanced(int rank, int max_rank)
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

    /* O calculo ja e cronometrado dentro de sort_balanced_tree. */
    MPI_Recv(vetor, tam, MPI_INT, parent, TAG_WORK, MPI_COMM_WORLD, &status);
    sort_balanced_tree(rank, max_rank, vetor, tam, topmost_level(rank));
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
    max_rank = nprocs - 1;

    if (argc >= 2) {
        tam = atoi(argv[1]);
    }
    if (argc >= 3) {
        input_mode = parse_input_mode(argv[2]);
    }
    if (tam <= 0) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s [tamanho] [reverse|random|almost]\n", argv[0]);
        }
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    if (rank == 0) {
        vetor = (int *)malloc((size_t)tam * sizeof(int));
        die_if_null(vetor, "vetor");
        init_vector(vetor, tam, input_mode);

        MPI_Barrier(MPI_COMM_WORLD);
        inicio = MPI_Wtime();
        sort_balanced_tree(rank, max_rank, vetor, tam, 0);
        fim = MPI_Wtime();

        /* Sinaliza o fim do trabalho aos demais processos. */
        for (i = 1; i < nprocs; i++) {
            MPI_Send(NULL, 0, MPI_INT, i, TAG_STOP, MPI_COMM_WORLD);
        }
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        run_helper_balanced(rank, max_rank);
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
        printf("programa=mpi_balanceado tamanho=%d processos=%d entrada=%d tempo=%.6f ordenado=%d tempo_min=%.6f tempo_max=%.6f tempo_medio=%.6f desbalanceamento=%.6f segmentos=%d processos_ativos=%d\n",
               tam, nprocs, input_mode, fim - inicio, is_sorted(vetor, tam),
               tempo_minimo, tempo_maximo,
               ativos_soma > 0 ? tempo_soma / ativos_soma : 0.0,
               tempo_maximo > 0.0 ? (tempo_maximo - tempo_minimo) / tempo_maximo : 0.0,
               segmentos_soma, ativos_soma);
        free(vetor);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
