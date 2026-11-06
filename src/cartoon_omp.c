#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "../include/ppm_utils.h"
#include "../include/cartoon_core.h"

void print_usage(const char *prog) {
    printf("Uso: %s <input.ppm | --synthetic> [output.ppm] [num_threads] [filter_size: 3|5] [k_levels: 3|9] [sobel_threshold] [width] [height]\n", prog);
    printf("Ejemplos:\n");
    printf("  %s input.ppm output_omp.ppm 4 3 9 70\n", prog);
    printf("  %s --synthetic output_omp.ppm 8 5 3 80 2000 2000\n", prog);
}

// OpenMP accelerated low-pass filter
void omp_apply_mean_filter_rgb(
    const PixelRGB *src, 
    PixelRGB *dst, 
    int width, 
    int height, 
    int filter_size) 
{
    int radius = filter_size / 2;
    int num_pixels_window = filter_size * filter_size;

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int sum_r = 0, sum_g = 0, sum_b = 0;

            for (int dy = -radius; dy <= radius; ++dy) {
                int ny = y + dy;
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        int s_idx = ny * width + nx;
                        sum_r += src[s_idx].r;
                        sum_g += src[s_idx].g;
                        sum_b += src[s_idx].b;
                    }
                }
            }

            int d_idx = y * width + x;
            dst[d_idx].r = (uint8_t)(sum_r / num_pixels_window);
            dst[d_idx].g = (uint8_t)(sum_g / num_pixels_window);
            dst[d_idx].b = (uint8_t)(sum_b / num_pixels_window);
        }
    }
}

// OpenMP accelerated Sobel edge detection
void omp_apply_sobel_edge_detection(
    const PixelRGB *src, 
    uint8_t *edge_out, 
    int width, 
    int height, 
    int threshold) 
{
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int gx = 0;
            int gy = 0;

            for (int dy = -1; dy <= 1; ++dy) {
                int ny = y + dy;
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx;
                    int intensity = 0;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        intensity = rgb_to_gray(src[ny * width + nx]);
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
            edge_out[y * width + x] = (mag >= threshold) ? 255 : 0;
        }
    }
}

// OpenMP accelerated posterization
void omp_apply_posterization(
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

// OpenMP accelerated saturated addition
void omp_apply_saturated_addition(
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
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = (argc > 2) ? argv[2] : "output_omp.ppm";
    int num_threads = (argc > 3) ? atoi(argv[3]) : 4;
    int filter_size = (argc > 4) ? atoi(argv[4]) : 3;
    int k_levels = (argc > 5) ? atoi(argv[5]) : 9;
    int threshold = (argc > 6) ? atoi(argv[6]) : 70;
    int syn_width = (argc > 7) ? atoi(argv[7]) : 800;
    int syn_height = (argc > 8) ? atoi(argv[8]) : 800;

    if (num_threads <= 0) {
        num_threads = omp_get_max_threads();
    }
    omp_set_num_threads(num_threads);

    if (filter_size != 3 && filter_size != 5) {
        fprintf(stderr, "Aviso: filter_size debe ser 3 o 5. Ajustando a 3.\n");
        filter_size = 3;
    }

    ImagePPM *input_img = NULL;
    if (strcmp(input_path, "--synthetic") == 0) {
        input_img = ppm_generate_synthetic(syn_width, syn_height);
    } else {
        input_img = ppm_read(input_path);
    }

    if (!input_img) {
        fprintf(stderr, "Error cargando la imagen de entrada.\n");
        return 1;
    }

    int width = input_img->width;
    int height = input_img->height;
    int total_pixels = width * height;

    printf("[OMP] Hilos: %d | Tamano: %dx%d | Filtro: %dx%d | K: %d | Umbral: %d\n",
           num_threads, width, height, filter_size, filter_size, k_levels, threshold);

    PixelRGB *blurred_img = (PixelRGB*)malloc((size_t)total_pixels * sizeof(PixelRGB));
    uint8_t *edge_mask = (uint8_t*)malloc((size_t)total_pixels * sizeof(uint8_t));
    PixelRGB *posterized_img = (PixelRGB*)malloc((size_t)total_pixels * sizeof(PixelRGB));
    ImagePPM *output_img = ppm_create(width, height);

    if (!blurred_img || !edge_mask || !posterized_img || !output_img) {
        fprintf(stderr, "Error asignando memoria para los buffers OMP.\n");
        return 1;
    }

    double t_start = omp_get_wtime();

    // 1. Filtrado pasa-bajos
    omp_apply_mean_filter_rgb(input_img->data, blurred_img, width, height, filter_size);

    // 2. Sobel + umbralado
    omp_apply_sobel_edge_detection(blurred_img, edge_mask, width, height, threshold);

    // 3. Posterizado
    omp_apply_posterization(input_img->data, posterized_img, total_pixels, k_levels);

    // 4. Suma saturada
    omp_apply_saturated_addition(posterized_img, edge_mask, output_img->data, total_pixels);

    double t_end = omp_get_wtime();
    double elapsed_time = t_end - t_start;

    printf("[OMP] Tiempo total de computo: %.6f segundos (%.2f MPixels/s)\n",
           elapsed_time, ((double)total_pixels / 1e6) / elapsed_time);

    if (output_path && strlen(output_path) > 0 && strcmp(output_path, "none") != 0) {
        ppm_write(output_path, output_img);
        printf("[OMP] Resultado guardado en: %s\n", output_path);
    }

    free(blurred_img);
    free(edge_mask);
    free(posterized_img);
    ppm_free(input_img);
    ppm_free(output_img);

    return 0;
}
