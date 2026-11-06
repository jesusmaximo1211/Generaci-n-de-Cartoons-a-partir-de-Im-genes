#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/ppm_utils.h"
#include "../include/cartoon_core.h"

// High-resolution timer in seconds
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void print_usage(const char *prog) {
    printf("Uso: %s <input.ppm | --synthetic> [output.ppm] [filter_size: 3|5] [k_levels: 3|9] [sobel_threshold] [width] [height]\n", prog);
    printf("Ejemplos:\n");
    printf("  %s input.ppm output_seq.ppm 3 9 70\n", prog);
    printf("  %s --synthetic output_seq.ppm 5 3 80 2000 2000\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = (argc > 2) ? argv[2] : "output_seq.ppm";
    int filter_size = (argc > 3) ? atoi(argv[3]) : 3;
    int k_levels = (argc > 4) ? atoi(argv[4]) : 9;
    int threshold = (argc > 5) ? atoi(argv[5]) : 70;
    int syn_width = (argc > 6) ? atoi(argv[6]) : 800;
    int syn_height = (argc > 7) ? atoi(argv[7]) : 800;

    if (filter_size != 3 && filter_size != 5) {
        fprintf(stderr, "Aviso: filter_size debe ser 3 o 5. Ajustando a 3.\n");
        filter_size = 3;
    }

    ImagePPM *input_img = NULL;
    if (strcmp(input_path, "--synthetic") == 0) {
        printf("[SEQ] Generando imagen sintetica de %dx%d...\n", syn_width, syn_height);
        input_img = ppm_generate_synthetic(syn_width, syn_height);
    } else {
        printf("[SEQ] Leyendo imagen %s...\n", input_path);
        input_img = ppm_read(input_path);
    }

    if (!input_img) {
        fprintf(stderr, "Error cargando la imagen de entrada.\n");
        return 1;
    }

    int width = input_img->width;
    int height = input_img->height;
    int total_pixels = width * height;

    printf("[SEQ] Parametros: %dx%d | Filtro: %dx%d | Posterizado K: %d | Umbral Sobel: %d\n",
           width, height, filter_size, filter_size, k_levels, threshold);

    // Buffers for pipeline
    PixelRGB *blurred_img = (PixelRGB*)malloc((size_t)total_pixels * sizeof(PixelRGB));
    uint8_t *edge_mask = (uint8_t*)malloc((size_t)total_pixels * sizeof(uint8_t));
    PixelRGB *posterized_img = (PixelRGB*)malloc((size_t)total_pixels * sizeof(PixelRGB));
    ImagePPM *output_img = ppm_create(width, height);

    if (!blurred_img || !edge_mask || !posterized_img || !output_img) {
        fprintf(stderr, "Error asignando memoria para los buffers de procesamiento.\n");
        return 1;
    }

    // Benchmark time
    double t_start = get_time_sec();

    // 1. Filtrado pasa-bajos de media
    apply_mean_filter_rgb(input_img->data, blurred_img, width, height, filter_size);

    // 2. Detección de bordes Sobel + umbralado sobre la imagen suavizada
    apply_sobel_edge_detection(blurred_img, edge_mask, width, height, threshold);

    // 3. Posterizado de la imagen original por cada canal RGB en K valores discretos
    apply_posterization(input_img->data, posterized_img, total_pixels, k_levels);

    // 4. Suma saturada (clamp 0-255) de bordes y posterizado
    apply_saturated_addition(posterized_img, edge_mask, output_img->data, total_pixels);

    double t_end = get_time_sec();
    double elapsed_time = t_end - t_start;

    printf("[SEQ] Tiempo total de computo: %.6f segundos (%.2f MPixels/s)\n",
           elapsed_time, ((double)total_pixels / 1e6) / elapsed_time);

    // Guardar imagen resultante
    if (output_path && strlen(output_path) > 0) {
        if (ppm_write(output_path, output_img)) {
            printf("[SEQ] Resultado guardado exitosamente en: %s\n", output_path);
        } else {
            fprintf(stderr, "[SEQ] Error guardando resultado en %s\n", output_path);
        }
    }

    // Liberar recursos
    free(blurred_img);
    free(edge_mask);
    free(posterized_img);
    ppm_free(input_img);
    ppm_free(output_img);

    return 0;
}
