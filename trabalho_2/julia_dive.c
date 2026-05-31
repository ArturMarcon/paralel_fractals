#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define WIDTH 1920
#define HEIGHT 1080
#define MAX_ITER 2000
#define TOTAL_FRAMES 300

/* Tags MPI. O resultado usa (TAG_RESULT_BASE + id do quadro) para evitar
 * colisao do quadro 0 com TAG_REQUEST. */
#define TAG_REQUEST 0
#define TAG_WORK    1
#define TAG_DIE     2
#define TAG_RESULT_BASE 3

const double C_REAL = -0.7;
const double C_IMAG = 0.27015;

/* Tres cores frias: Azul Noite -> Ciano Eletrico -> Roxo Profundo */
const int COLOR1_R = 10;   const int COLOR1_G = 15;   const int COLOR1_B = 45;
const int COLOR2_R = 0;    const int COLOR2_G = 225;  const int COLOR2_B = 235;
const int COLOR3_R = 140;  const int COLOR3_G = 25;   const int COLOR3_B = 230;

/* Calcula um quadro (unidade de trabalho) gerando os pixels RGB no buffer */
void compute_julia_frame(int frame_id, double center_real, double center_imag, unsigned char *buffer) {
    double zoom = pow(0.94, frame_id);

    double base_width = 3.0;
    double current_width = base_width * zoom;
    double current_height = current_width * ((double)HEIGHT / (double)WIDTH);

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            double z_real = center_real + (x - WIDTH / 2.0) * (current_width / WIDTH);
            double z_imag = center_imag + (y - HEIGHT / 2.0) * (current_height / HEIGHT);

            int iter = 0;
            double z_real_sq = z_real * z_real;
            double z_imag_sq = z_imag * z_imag;

            while (z_real_sq + z_imag_sq <= 4.0 && iter < MAX_ITER) {
                double temp = z_real_sq - z_imag_sq + C_REAL;
                z_imag = 2.0 * z_real * z_imag + C_IMAG;
                z_real = temp;

                z_real_sq = z_real * z_real;
                z_imag_sq = z_imag * z_imag;
                iter++;
            }

            size_t pixel_idx = (size_t)(y * WIDTH + x) * 3;

            if (iter == MAX_ITER) {
                buffer[pixel_idx]     = 0;
                buffer[pixel_idx + 1] = 0;
                buffer[pixel_idx + 2] = 0;
            } else {
                double mu = (double)iter / MAX_ITER;

                if (mu < 0.5) {
                    double t = mu * 2.0;
                    buffer[pixel_idx]     = (unsigned char)((1.0 - t) * COLOR1_R + t * COLOR2_R);
                    buffer[pixel_idx + 1] = (unsigned char)((1.0 - t) * COLOR1_G + t * COLOR2_G);
                    buffer[pixel_idx + 2] = (unsigned char)((1.0 - t) * COLOR1_B + t * COLOR2_B);
                } else {
                    double t = (mu - 0.5) * 2.0;
                    buffer[pixel_idx]     = (unsigned char)((1.0 - t) * COLOR2_R + t * COLOR3_R);
                    buffer[pixel_idx + 1] = (unsigned char)((1.0 - t) * COLOR2_G + t * COLOR3_G);
                    buffer[pixel_idx + 2] = (unsigned char)((1.0 - t) * COLOR2_B + t * COLOR3_B);
                }
            }
        }
    }
}

void save_ppm(int frame_id, unsigned char *buffer) {
    char filename[64];
    sprintf(filename, "frame_%04d.ppm", frame_id);

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Erro ao abrir o arquivo %s para escrita.\n", filename);
        return;
    }

    fprintf(fp, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    fwrite(buffer, sizeof(unsigned char), (size_t)WIDTH * HEIGHT * 3, fp);
    fclose(fp);
}

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double target_real = -0.1;
    double target_imag = -0.1;
    int total_frames = TOTAL_FRAMES;

    /* Args opcionais: <real> <imag> [nframes].
     * nframes permite variar o tamanho do problema para o speed-up fraco. */
    if (argc >= 3) {
        target_real = atof(argv[1]);
        target_imag = atof(argv[2]);
    }
    if (argc >= 4) {
        total_frames = atoi(argv[3]);
    }

    size_t frame_buffer_size = (size_t)WIDTH * HEIGHT * 3 * sizeof(unsigned char);

    /* np == 1: modo sequencial, linha de base para o Speed-Up */
    if (size == 1) {
        printf("[Sequencial] Renderizando %d quadros no alvo (%f, %f)\n",
               total_frames, target_real, target_imag);
        double start_time = MPI_Wtime();

        unsigned char *buffer = (unsigned char *)malloc(frame_buffer_size);
        for (int f = 0; f < total_frames; f++) {
            compute_julia_frame(f, target_real, target_imag, buffer);
            save_ppm(f, buffer);
        }
        free(buffer);

        double end_time = MPI_Wtime();
        printf("[Sequencial] Concluido em %.4f segundos.\n", end_time - start_time);

        MPI_Finalize();
        return 0;
    }

    if (rank == 0) {
        /* Coordenador: gerencia o saco de trabalho, distribui sob demanda e
         * grava os resultados na ordem de chegada (sem calcular quadros). */
        printf("[Mestre] Distribuindo %d quadros entre %d trabalhadores. Alvo: (%f, %f)\n",
               total_frames, size - 1, target_real, target_imag);

        MPI_Barrier(MPI_COMM_WORLD); /* sincroniza todos antes de cronometrar */
        double start_time = MPI_Wtime();

        int next_frame = 0;
        int frames_saved = 0;
        int active_workers = size - 1;

        int *work_count = (int *)calloc(size, sizeof(int)); /* quadros por trabalhador */
        unsigned char *recv_buffer = (unsigned char *)malloc(frame_buffer_size);

        while (frames_saved < total_frames || active_workers > 0) {
            MPI_Status status;

            /* Espia a mensagem; do mesmo par, o resultado chega antes do proximo pedido */
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            int worker_id = status.MPI_SOURCE;

            if (status.MPI_TAG == TAG_REQUEST) {
                int dummy;
                MPI_Recv(&dummy, 1, MPI_INT, worker_id, TAG_REQUEST,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                if (next_frame < total_frames) {
                    MPI_Send(&next_frame, 1, MPI_INT, worker_id, TAG_WORK, MPI_COMM_WORLD);
                    next_frame++;
                } else {
                    int term = -1;
                    MPI_Send(&term, 1, MPI_INT, worker_id, TAG_DIE, MPI_COMM_WORLD);
                    active_workers--;
                }
            } else {
                int completed_frame_id = status.MPI_TAG - TAG_RESULT_BASE;
                MPI_Recv(recv_buffer, (int)frame_buffer_size, MPI_BYTE, worker_id,
                         status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                save_ppm(completed_frame_id, recv_buffer);
                frames_saved++;
                work_count[worker_id]++;
            }
        }

        double end_time = MPI_Wtime();
        printf("[Mestre] Concluido em %.4f segundos.\n", end_time - start_time);

        /* Relatorio de balanceamento de carga */
        int min_w = total_frames, max_w = 0;
        printf("[Mestre] Quadros processados por trabalhador:\n");
        for (int w = 1; w < size; w++) {
            printf("         trabalhador %d: %d quadros\n", w, work_count[w]);
            if (work_count[w] < min_w) min_w = work_count[w];
            if (work_count[w] > max_w) max_w = work_count[w];
        }
        printf("[Mestre] Balanceamento -> min: %d | max: %d | media: %.2f quadros/trabalhador\n",
               min_w, max_w, (double)total_frames / (size - 1));

        free(work_count);
        free(recv_buffer);

    } else {
        /* Trabalhador: toma a iniciativa de pedir, processa e devolve */
        unsigned char *local_buffer = (unsigned char *)malloc(frame_buffer_size);

        MPI_Barrier(MPI_COMM_WORLD); /* sincroniza com o mestre antes de cronometrar */

        while (1) {
            int frame_to_build;
            MPI_Status status;

            int request = rank;
            MPI_Send(&request, 1, MPI_INT, 0, TAG_REQUEST, MPI_COMM_WORLD);

            MPI_Recv(&frame_to_build, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == TAG_DIE) {
                break;
            }

            compute_julia_frame(frame_to_build, target_real, target_imag, local_buffer);

            MPI_Send(local_buffer, (int)frame_buffer_size, MPI_BYTE, 0,
                     TAG_RESULT_BASE + frame_to_build, MPI_COMM_WORLD);
        }

        free(local_buffer);
    }

    MPI_Finalize();
    return 0;
}
