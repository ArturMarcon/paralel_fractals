#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define WIDTH 1920
#define HEIGHT 1080
#define MAX_ITER 2000
#define TOTAL_FRAMES 300

// MPI Message Tags
#define TAG_WORK 1
#define TAG_DIE 2

// Julia set constant configuration
const double C_REAL = -0.7;
const double C_IMAG = 0.27015;

// Define 3 cool colors matching the first program: Night Blue -> Electric Cyan -> Deep Purple
const int COLOR1_R = 10;   const int COLOR1_G = 15;   const int COLOR1_B = 45;   // Night Blue
const int COLOR2_R = 0;    const int COLOR2_G = 225;  const int COLOR2_B = 235;  // Electric Cyan
const int COLOR3_R = 140;  const int COLOR3_G = 25;   const int COLOR3_B = 230;  // Deep Purple

// Computes a single frame's pixel data into a raw RGB buffer
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
                // Fractal interior (Black)
                buffer[pixel_idx]     = 0;   
                buffer[pixel_idx + 1] = 0;
                buffer[pixel_idx + 2] = 0;
            } else {
                // Normalized iteration value for smooth 3-color transition
                double mu = (double)iter / MAX_ITER;

                if (mu < 0.5) {
                    double t = mu * 2.0; // Scale to [0, 1]
                    buffer[pixel_idx]     = (unsigned char)((1.0 - t) * COLOR1_R + t * COLOR2_R);
                    buffer[pixel_idx + 1] = (unsigned char)((1.0 - t) * COLOR1_G + t * COLOR2_G);
                    buffer[pixel_idx + 2] = (unsigned char)((1.0 - t) * COLOR1_B + t * COLOR2_B);
                } else {
                    double t = (mu - 0.5) * 2.0; // Scale to [0, 1]
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
        fprintf(stderr, "Error opening file %s for writing.\n", filename);
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

    if (argc == 3) {
        target_real = atof(argv[1]);
        target_imag = atof(argv[2]);
    }

    if (size < 2) {
        if (rank == 0) {
            fprintf(stderr, "Error: This Master-Worker model requires at least 2 MPI processes.\n");
        }
        MPI_Finalize();
        return 1;
    }

    size_t frame_buffer_size = (size_t)WIDTH * HEIGHT * 3 * sizeof(unsigned char);

    if (rank == 0) {
        // ================= MASTER RANK =================
        printf("[Master] Rendering smooth gradient frames into coordinates: (%f, %f)\n", target_real, target_imag);
        double start_time = MPI_Wtime();

        int next_frame = 0;
        int active_workers = size - 1;

        for (int worker = 1; worker < size; worker++) {
            if (next_frame < TOTAL_FRAMES) {
                MPI_Send(&next_frame, 1, MPI_INT, worker, TAG_WORK, MPI_COMM_WORLD);
                next_frame++;
            } else {
                MPI_Send(&next_frame, 1, MPI_INT, worker, TAG_DIE, MPI_COMM_WORLD);
                active_workers--;
            }
        }

        unsigned char *recv_buffer = (unsigned char *)malloc(frame_buffer_size);
        while (active_workers > 0) {
            MPI_Status status;
            
            MPI_Recv(recv_buffer, (int)frame_buffer_size, MPI_BYTE, MPI_ANY_SOURCE, 
                     MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            
            int worker_id = status.MPI_SOURCE;
            int completed_frame_id = status.MPI_TAG; 

            save_ppm(completed_frame_id, recv_buffer);

            if (next_frame < TOTAL_FRAMES) {
                MPI_Send(&next_frame, 1, MPI_INT, worker_id, TAG_WORK, MPI_COMM_WORLD);
                next_frame++;
            } else {
                MPI_Send(&next_frame, 1, MPI_INT, worker_id, TAG_DIE, MPI_COMM_WORLD);
                active_workers--;
            }
        }

        free(recv_buffer);
        double end_time = MPI_Wtime();
        printf("[Master] Completed in %.4f seconds.\n", end_time - start_time);

    } else {
        // ================= WORKER RANKS =================
        unsigned char *local_buffer = (unsigned char *)malloc(frame_buffer_size);

        while (1) {
            int frame_to_build;
            MPI_Status status;

            MPI_Recv(&frame_to_build, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_DIE) {
                break; 
            }

            compute_julia_frame(frame_to_build, target_real, target_imag, local_buffer);

            MPI_Send(local_buffer, (int)frame_buffer_size, MPI_BYTE, 0, frame_to_build, MPI_COMM_WORLD);
        }

        free(local_buffer);
    }

    MPI_Finalize();
    return 0;
}
