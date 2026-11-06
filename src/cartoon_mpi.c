#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "../include/ppm_utils.h"
#include "../include/cartoon_core.h"

void print_usage(const char *prog) {
    printf("Uso: mpirun -np <P> %s <input.ppm | --synthetic> [output.ppm] [filter_size: 3|5] [k_levels: 3|9] [sobel_threshold] [width] [height]\n", prog);
}

// Compute 1D row decomposition across P processes
// Handles cases where height is not divisible by P
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

// Exchange halos for an RGB image chunk with halo rows
// local_buf has dimension: (num_rows + 2 * halo_radius) * width
// Process 0 has top boundary as physical image edge (or padding with zeros)
// Process (size-1) has bottom boundary as physical image edge (or padding with zeros)
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

    // Pointers inside local_buf:
    // [0 .. halo_pixels-1]: Top halo
    // [halo_pixels .. halo_pixels + num_rows*width - 1]: Interior data
    // [halo_pixels + (num_rows)*width .. ]: Bottom halo

    PixelRGB *top_halo = local_buf;
    PixelRGB *top_inner = local_buf + halo_pixels;
    PixelRGB *bottom_inner = local_buf + halo_pixels + (num_rows - halo_radius) * width;
    PixelRGB *bottom_halo = local_buf + halo_pixels + num_rows * width;

    // If top boundary is global edge (rank 0), fill top halo with neutral black (0,0,0)
    if (rank == 0) {
        memset(top_halo, 0, halo_bytes);
    }
    // If bottom boundary is global edge (rank size - 1), fill bottom halo with neutral black (0,0,0)
    if (rank == size - 1) {
        memset(bottom_halo, 0, halo_bytes);
    }

    // 1. Send top inner rows to up_neighbor, receive bottom halo from down_neighbor
    MPI_Sendrecv(top_inner, halo_pixels * 3, MPI_BYTE, up_neighbor, 10,
                 bottom_halo, halo_pixels * 3, MPI_BYTE, down_neighbor, 10,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // 2. Send bottom inner rows to down_neighbor, receive top halo from up_neighbor
    MPI_Sendrecv(bottom_inner, halo_pixels * 3, MPI_BYTE, down_neighbor, 20,
                 top_halo, halo_pixels * 3, MPI_BYTE, up_neighbor, 20,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

// Mean filter on local block with halo
// halo_buf has (num_rows + 2*radius) rows.
// Output filtered_buf has num_rows (only local domain).
static void local_apply_mean_filter_rgb(
    const PixelRGB *halo_buf, 
    PixelRGB *filtered_buf, 
    int num_rows, 
    int width, 
    int filter_size) 
{
    int radius = filter_size / 2;
    int num_pixels_window = filter_size * filter_size;

    for (int r = 0; r < num_rows; ++r) {
        int halo_y = r + radius; // coordinate in halo_buf
        for (int x = 0; x < width; ++x) {
            int sum_r = 0, sum_g = 0, sum_b = 0;

            for (int dy = -radius; dy <= radius; ++dy) {
                int ny = halo_y + dy;
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx;
                    // Neutral black border if x is out of bounds
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

// Sobel Edge Detection on local block with 1-row halo
// halo_buf has (num_rows + 2*1) rows.
// Output edge_buf has num_rows.
static void local_apply_sobel_edge_detection(
    const PixelRGB *halo_buf, 
    uint8_t *edge_buf, 
    int num_rows, 
    int width, 
    int threshold) 
{
    int radius = 1;
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

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) print_usage(argv[0]);
        MPI_Finalize();
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = (argc > 2) ? argv[2] : "output_mpi.ppm";
    int filter_size = (argc > 3) ? atoi(argv[3]) : 3;
    int k_levels = (argc > 4) ? atoi(argv[4]) : 9;
    int threshold = (argc > 5) ? atoi(argv[5]) : 70;
    int syn_width = (argc > 6) ? atoi(argv[6]) : 800;
    int syn_height = (argc > 7) ? atoi(argv[7]) : 800;

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
            fprintf(stderr, "[MPI Rank 0] Error cargando/generando imagen.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        width = full_img->width;
        height = full_img->height;
        printf("[MPI] Procesos: %d | Resolucion: %dx%d | Filtro: %dx%d | Posterizado K: %d | Sobel: %d\n",
               size, width, height, filter_size, filter_size, k_levels, threshold);
    }

    // Broadcast dimensions
    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Compute row decomposition
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

    // Halo radius for mean filter: 1 for 3x3, 2 for 5x5
    int mean_radius = filter_size / 2;
    int halo_mean_total_rows = local_rows + 2 * mean_radius;

    // Buffer for local raw image with mean halo
    PixelRGB *raw_with_mean_halo = (PixelRGB*)calloc((size_t)halo_mean_total_rows * width, sizeof(PixelRGB));
    PixelRGB *raw_local_inner = raw_with_mean_halo + mean_radius * width;

    // Local blurred buffer (domain only)
    PixelRGB *blurred_local = (PixelRGB*)malloc((size_t)local_pixels * sizeof(PixelRGB));

    // Local blurred buffer with Sobel halo (radius 1 = 1 row top, 1 row bottom)
    int sobel_radius = 1;
    int halo_sobel_total_rows = local_rows + 2 * sobel_radius;
    PixelRGB *blurred_with_sobel_halo = (PixelRGB*)calloc((size_t)halo_sobel_total_rows * width, sizeof(PixelRGB));
    PixelRGB *blurred_local_inner = blurred_with_sobel_halo + sobel_radius * width;

    // Edge mask and posterized buffers
    uint8_t *edge_mask_local = (uint8_t*)malloc((size_t)local_pixels * sizeof(uint8_t));
    PixelRGB *posterized_local = (PixelRGB*)malloc((size_t)local_pixels * sizeof(PixelRGB));
    PixelRGB *cartoon_local = (PixelRGB*)malloc((size_t)local_pixels * sizeof(PixelRGB));

    // Scatter original raw image partition into raw_local_inner
    MPI_Scatterv(rank == 0 ? full_img->data : NULL,
                 sendcounts_bytes, displs_bytes, MPI_BYTE,
                 raw_local_inner, local_pixels * sizeof(PixelRGB), MPI_BYTE,
                 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    // STEP 1: Exchange halos for raw image (halo_radius = mean_radius: 1 or 2)
    exchange_halos_rgb(raw_with_mean_halo, local_rows, width, mean_radius, rank, size);

    // STEP 2: Compute local mean filter
    local_apply_mean_filter_rgb(raw_with_mean_halo, blurred_local, local_rows, width, filter_size);

    // STEP 3: Exchange halos for blurred image for Sobel filter (halo_radius = 1)
    // Copy blurred_local into inner part of blurred_with_sobel_halo
    memcpy(blurred_local_inner, blurred_local, local_pixels * sizeof(PixelRGB));
    exchange_halos_rgb(blurred_with_sobel_halo, local_rows, width, sobel_radius, rank, size);

    // STEP 4: Compute Sobel edge detection on blurred chunk with halo
    local_apply_sobel_edge_detection(blurred_with_sobel_halo, edge_mask_local, local_rows, width, threshold);

    // STEP 5: Posterize original raw local chunk
    apply_posterization(raw_local_inner, posterized_local, local_pixels, k_levels);

    // STEP 6: Saturated addition
    apply_saturated_addition(posterized_local, edge_mask_local, cartoon_local, local_pixels);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_end = MPI_Wtime();
    double local_elapsed = t_end - t_start;

    double max_elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Gatherv result at Rank 0
    ImagePPM *final_img = NULL;
    if (rank == 0) {
        final_img = ppm_create(width, height);
    }

    MPI_Gatherv(cartoon_local, local_pixels * sizeof(PixelRGB), MPI_BYTE,
                rank == 0 ? final_img->data : NULL,
                sendcounts_bytes, displs_bytes, MPI_BYTE,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("[MPI] Tiempo total de computo (max): %.6f segundos (%.2f MPixels/s)\n",
               max_elapsed, ((double)(width * height) / 1e6) / max_elapsed);

        if (output_path && strlen(output_path) > 0) {
            ppm_write(output_path, final_img);
            printf("[MPI] Resultado guardado exitosamente en: %s\n", output_path);
        }
        ppm_free(full_img);
        ppm_free(final_img);
    }

    // Cleanup local buffers
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
