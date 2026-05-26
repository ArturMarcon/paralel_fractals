#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define WIDTH 1920
#define HEIGHT 1080
#define MAX_ITER 2000
#define DEFAULT_TOTAL_FRAMES 300

const double C_REAL = -0.7;
const double C_IMAG = 0.27015;

const int COLOR1_R = 10;   const int COLOR1_G = 15;   const int COLOR1_B = 45;
const int COLOR2_R = 0;    const int COLOR2_G = 225;  const int COLOR2_B = 235;
const int COLOR3_R = 140;  const int COLOR3_G = 25;   const int COLOR3_B = 230;

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
        fprintf(stderr, "Error opening file %s for writing.\n", filename);
        return;
    }

    fprintf(fp, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    fwrite(buffer, sizeof(unsigned char), (size_t)WIDTH * HEIGHT * 3, fp);
    fclose(fp);
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    double target_real = -0.1;
    double target_imag = -0.1;
    int total_frames = DEFAULT_TOTAL_FRAMES;
    int save_files = 1;

    if (argc >= 3) {
        target_real = atof(argv[1]);
        target_imag = atof(argv[2]);
    }
    if (argc >= 4) {
        total_frames = atoi(argv[3]);
    }
    if (argc >= 5) {
        save_files = atoi(argv[4]);
    }

    size_t frame_buffer_size = (size_t)WIDTH * HEIGHT * 3 * sizeof(unsigned char);
    unsigned char *buffer = (unsigned char *)malloc(frame_buffer_size);
    if (!buffer) {
        fprintf(stderr, "Error: failed to allocate frame buffer.\n");
        return 1;
    }

    printf("[Seq] Rendering %d frames into (%f, %f), save_files=%d\n",
           total_frames, target_real, target_imag, save_files);

    double t_compute = 0.0;
    double t_io = 0.0;
    double t_start = now_seconds();

    for (int frame_id = 0; frame_id < total_frames; frame_id++) {
        double tc0 = now_seconds();
        compute_julia_frame(frame_id, target_real, target_imag, buffer);
        double tc1 = now_seconds();
        t_compute += (tc1 - tc0);

        if (save_files) {
            double ti0 = now_seconds();
            save_ppm(frame_id, buffer);
            double ti1 = now_seconds();
            t_io += (ti1 - ti0);
        }
    }

    double t_end = now_seconds();
    double t_total = t_end - t_start;

    printf("[Seq] total=%.4fs  compute=%.4fs  io=%.4fs\n", t_total, t_compute, t_io);

    free(buffer);
    return 0;
}
