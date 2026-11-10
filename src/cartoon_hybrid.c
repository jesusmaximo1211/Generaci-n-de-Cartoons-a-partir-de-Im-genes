#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include "../include/ppm_utils.h"
#include "../include/cartoon_core.h"

void print_usage(const char *prog) {
    printf("Uso: mpirun -np <P> %s <input.ppm | --synthetic> [output.ppm] [threads_per_proc] [filter_size: 3|5] [k_levels: 3|9] [sobel_threshold] [width] [height]\n", prog);
}

static void compute_row_partition(int height, int size, int *counts, int *displs) {
    int base = height / size;
    int rem = height % size;
    int offset = 0;
    for (int p = 0; p < size; ++p) {
        counts[p] = base + (p < rem ? 1 : 0);
        displs[p] = offset;
        offset += counts[p];
    }
}

// Halo exchange with MPI_Sendrecv
static void exchange_halos_rgb(
    PixelRGB *local_buf, 
    int num_rows, 
    int width, 
    int halo_radius, 
    int rank, 
    int size) 
{
    int row_bytes = width * sizeof(PixelRGB);
    int halo_bytes = halo_radius * row_bytes;
    int halo_pixels = halo_radius * width;

    int up_neighbor = (rank > 0) ? rank - 1 : MPI_PROC_NULL;
    int down_neighbor = (rank < size - 1) ? rank + 1 : MPI_PROC_NULL;

    PixelRGB *top_halo = local_buf;
    PixelRGB *top_inner = local_buf + halo_pixels;
    PixelRGB *bottom_inner = local_buf + halo_pixels + (num_rows - halo_radius) * width;
    PixelRGB *bottom_halo = local_buf + halo_pixels + num_rows * width;

    if (rank == 0) {
        memset(top_halo, 0, halo_bytes);
    }
    if (rank == size - 1) {
        memset(bottom_halo, 0, halo_bytes);
    }

    MPI_Sendrecv(top_inner, halo_pixels * 3, MPI_BYTE, up_neighbor, 100,
                 bottom_halo, halo_pixels * 3, MPI_BYTE, down_neighbor, 100,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MPI_Sendrecv(bottom_inner, halo_pixels * 3, MPI_BYTE, down_neighbor, 200,
                 top_halo, halo_pixels * 3, MPI_BYTE, up_neighbor, 200,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

// OpenMP accelerated local mean filter
static void hybrid_apply_mean_filter_rgb(
    const PixelRGB *halo_buf, 
    PixelRGB *filtered_buf, 
    int num_rows, 
    int width, 
    int filter_size) 
{
    int radius = filter_size / 2;
    int num_pixels_window = filter_size * filter_size;

    #pragma omp parallel for schedule(static)
    for (int r = 0; r < num_rows; ++r) {
        int halo_y = r + radius;
        for (int x = 0; x < width; ++x) {
            int sum_r = 0, sum_g = 0, sum_b = 0;

            for (int dy = -radius; dy <= radius; ++dy) {
                int ny = halo_y + dy;
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx;
                    if (nx >= 0 && nx < width) {
                        int idx = ny * width + nx;
                        sum_r += halo_buf[idx].r;
                        sum_g += halo_buf[idx].g;
                        sum_b += halo_buf[idx].b;
                    }
                }
            }

            int out_idx = r * width + x;
            filtered_buf[out_idx].r = (uint8_t)(sum_r / num_pixels_window);
            filtered_buf[out_idx].g = (uint8_t)(sum_g / num_pixels_window);
            filtered_buf[out_idx].b = (uint8_t)(sum_b / num_pixels_window);
        }
    }
}

// OpenMP accelerated local Sobel edge detection
static void hybrid_apply_sobel_edge_detection(
    const PixelRGB *halo_buf, 
    uint8_t *edge_buf, 
    int num_rows, 
    int width, 
    int threshold) 
{
    int radius = 1;
    #pragma omp parallel for schedule(static)
    for (int r = 0; r < num_rows; ++r) {
        int halo_y = r + radius;
        for (int x = 0; x < width; ++x) {
            int gx = 0;
            int gy = 0;

            for (int dy = -1; dy <= 1; ++dy) {
                int ny = halo_y + dy;
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx;
                    int intensity = 0;
                    if (nx >= 0 && nx < width) {
                        intensity = rgb_to_gray(halo_buf[ny * width + nx]);
                    }

                    int wx = 0, wy = 0;
                    if (dy == -1) {
                        if (dx == -1) { wx = -1; wy = -1; }
                        else if (dx == 0) { wx = 0; wy = -2; }
                        else if (dx == 1) { wx = 1; wy = -1; }
                    } else if (dy == 0) {
                        if (dx == -1) { wx = -2; wy = 0; }
                        else if (dx == 0) { wx = 0; wy = 0; }
                        else if (dx == 1) { wx = 2; wy = 0; }
                    } else if (dy == 1) {
                        if (dx == -1) { wx = -1; wy = 1; }
                        else if (dx == 0) { wx = 0; wy = 2; }
                        else if (dx == 1) { wx = 1; wy = 1; }
                    }

                    gx += wx * intensity;
                    gy += wy * intensity;
                }
            }

            int mag = (int)sqrt((double)(gx * gx + gy * gy));
            edge_buf[r * width + x] = (mag >= threshold) ? 255 : 0;
        }
    }
}

// OpenMP accelerated local posterization
static void hybrid_apply_posterization(
    const PixelRGB *src, 
    PixelRGB *dst, 
    int total_pixels, 
    int k_levels) 
{
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < total_pixels; ++i) {
        dst[i].r = quantize_channel(src[i].r, k_levels);
        dst[i].g = quantize_channel(src[i].g, k_levels);
        dst[i].b = quantize_channel(src[i].b, k_levels);
    }
}

// OpenMP accelerated local saturated addition
static void hybrid_apply_saturated_addition(
    const PixelRGB *posterized, 
    const uint8_t *edges, 
    PixelRGB *dst, 
    int total_pixels) 
{
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < total_pixels; ++i) {
        uint8_t edge_val = edges[i];
        dst[i].r = clamp_u8((int)posterized[i].r + edge_val);
        dst[i].g = clamp_u8((int)posterized[i].g + edge_val);
        dst[i].b = clamp_u8((int)posterized[i].b + edge_val);
    }
}

int main(int argc, char *argv[]) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) print_usage(argv[0]);
        MPI_Finalize();
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = (argc > 2) ? argv[2] : "output_hybrid.ppm";
    int threads_per_proc = (argc > 3) ? atoi(argv[3]) : 2;
    int filter_size = (argc > 4) ? atoi(argv[4]) : 3;
    int k_levels = (argc > 5) ? atoi(argv[5]) : 9;
    int threshold = (argc > 6) ? atoi(argv[6]) : 70;
    int syn_width = (argc > 7) ? atoi(argv[7]) : 800;
    int syn_height = (argc > 8) ? atoi(argv[8]) : 800;

    if (threads_per_proc <= 0) {
        threads_per_proc = omp_get_max_threads();
    }
    omp_set_num_threads(threads_per_proc);

    if (filter_size != 3 && filter_size != 5) {
        if (rank == 0) fprintf(stderr, "Aviso: filter_size debe ser 3 o 5. Usando 3.\n");
        filter_size = 3;
    }

    int width = 0;
    int height = 0;
    ImagePPM *full_img = NULL;

    if (rank == 0) {
        if (strcmp(input_path, "--synthetic") == 0) {
            full_img = ppm_generate_synthetic(syn_width, syn_height);
        } else {
            full_img = ppm_read(input_path);
        }
        if (!full_img) {
            fprintf(stderr, "[Hybrid Rank 0] Error cargando/generando imagen.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        width = full_img->width;
        height = full_img->height;
        printf("[HYBRID] Procesos MPI: %d | OMP Threads/Proc: %d (Total cores: %d) | %dx%d | Filtro: %dx%d | K: %d\n",
               size, threads_per_proc, size * threads_per_proc, width, height, filter_size, filter_size, k_levels);
    }

    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int *row_counts = (int*)malloc(size * sizeof(int));
    int *row_displs = (int*)malloc(size * sizeof(int));
    int *sendcounts_bytes = (int*)malloc(size * sizeof(int));
    int *displs_bytes = (int*)malloc(size * sizeof(int));

    compute_row_partition(height, size, row_counts, row_displs);

    for (int p = 0; p < size; ++p) {
        sendcounts_bytes[p] = row_counts[p] * width * sizeof(PixelRGB);
        displs_bytes[p] = row_displs[p] * width * sizeof(PixelRGB);
    }

    int local_rows = row_counts[rank];
    int local_pixels = local_rows * width;

    int mean_radius = filter_size / 2;
    int halo_mean_total_rows = local_rows + 2 * mean_radius;

    PixelRGB *raw_with_mean_halo = (PixelRGB*)calloc((size_t)halo_mean_total_rows * width, sizeof(PixelRGB));
    PixelRGB *raw_local_inner = raw_with_mean_halo + mean_radius * width;

    PixelRGB *blurred_local = (PixelRGB*)malloc((size_t)local_pixels * sizeof(PixelRGB));

    int sobel_radius = 1;
    int halo_sobel_total_rows = local_rows + 2 * sobel_radius;
    PixelRGB *blurred_with_sobel_halo = (PixelRGB*)calloc((size_t)halo_sobel_total_rows * width, sizeof(PixelRGB));
    PixelRGB *blurred_local_inner = blurred_with_sobel_halo + sobel_radius * width;

    uint8_t *edge_mask_local = (uint8_t*)malloc((size_t)local_pixels * sizeof(uint8_t));
    PixelRGB *posterized_local = (PixelRGB*)malloc((size_t)local_pixels * sizeof(PixelRGB));
    PixelRGB *cartoon_local = (PixelRGB*)malloc((size_t)local_pixels * sizeof(PixelRGB));

    ImagePPM *final_img = NULL;
    if (rank == 0) {
        final_img = ppm_create(width, height);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t_total_start = MPI_Wtime();

    MPI_Scatterv(rank == 0 ? full_img->data : NULL,
                 sendcounts_bytes, displs_bytes, MPI_BYTE,
                 raw_local_inner, local_pixels * sizeof(PixelRGB), MPI_BYTE,
                 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    // 1. Halo exchange for mean filter
    exchange_halos_rgb(raw_with_mean_halo, local_rows, width, mean_radius, rank, size);

    // 2. Mean filter using OpenMP
    hybrid_apply_mean_filter_rgb(raw_with_mean_halo, blurred_local, local_rows, width, filter_size);

    // 3. Halo exchange for Sobel filter
    memcpy(blurred_local_inner, blurred_local, local_pixels * sizeof(PixelRGB));
    exchange_halos_rgb(blurred_with_sobel_halo, local_rows, width, sobel_radius, rank, size);

    // 4. Sobel filter using OpenMP
    hybrid_apply_sobel_edge_detection(blurred_with_sobel_halo, edge_mask_local, local_rows, width, threshold);

    // 5. Posterization using OpenMP
    hybrid_apply_posterization(raw_local_inner, posterized_local, local_pixels, k_levels);

    // 6. Saturated addition using OpenMP
    hybrid_apply_saturated_addition(posterized_local, edge_mask_local, cartoon_local, local_pixels);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_end = MPI_Wtime();
    double local_elapsed = t_end - t_start;

    double max_elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Gatherv(cartoon_local, local_pixels * sizeof(PixelRGB), MPI_BYTE,
                rank == 0 ? final_img->data : NULL,
                sendcounts_bytes, displs_bytes, MPI_BYTE,
                0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_total_end = MPI_Wtime();
    double total_hyb_elapsed = t_total_end - t_total_start;

    if (rank == 0) {
        printf("[HYBRID] Tiempo total de computo (max): %.6f segundos (%.2f MPixels/s)\n",
               total_hyb_elapsed, ((double)(width * height) / 1e6) / total_hyb_elapsed);
        printf("[HYBRID] Computo+Halos local (max): %.6f segundos\n", max_elapsed);

        if (output_path && strlen(output_path) > 0 && strcmp(output_path, "none") != 0) {
            ppm_write(output_path, final_img);
            printf("[HYBRID] Resultado guardado exitosamente en: %s\n", output_path);
        }
        ppm_free(full_img);
        ppm_free(final_img);
    }

    free(row_counts);
    free(row_displs);
    free(sendcounts_bytes);
    free(displs_bytes);
    free(raw_with_mean_halo);
    free(blurred_local);
    free(blurred_with_sobel_halo);
    free(edge_mask_local);
    free(posterized_local);
    free(cartoon_local);

    MPI_Finalize();
    return 0;
}
