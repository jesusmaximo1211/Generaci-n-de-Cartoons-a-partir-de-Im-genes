#ifndef PPM_UTILS_H
#define PPM_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} PixelRGB;
#pragma pack(pop)

typedef struct {
    int width;
    int height;
    int max_color;
    PixelRGB *data; // Linear array: row-major order [height * width]
} ImagePPM;

// Skip comments and whitespaces in PPM header
static inline void ppm_skip_comments(FILE *fp) {
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (isspace(ch)) {
            continue;
        }
        if (ch == '#') {
            while ((ch = fgetc(fp)) != EOF && ch != '\n' && ch != '\r');
            continue;
        }
        ungetc(ch, fp);
        break;
    }
}

// Allocate an image buffer
static inline ImagePPM* ppm_create(int width, int height) {
    ImagePPM *img = (ImagePPM*)malloc(sizeof(ImagePPM));
    if (!img) return NULL;
    img->width = width;
    img->height = height;
    img->max_color = 255;
    img->data = (PixelRGB*)malloc((size_t)width * height * sizeof(PixelRGB));
    if (!img->data) {
        free(img);
        return NULL;
    }
    return img;
}

// Free an image
static inline void ppm_free(ImagePPM *img) {
    if (img) {
        if (img->data) free(img->data);
        free(img);
    }
}

// Read PPM (P6 binary format)
static inline ImagePPM* ppm_read(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error abriendo archivo PPM: %s\n", filename);
        return NULL;
    }

    char magic[3];
    if (fscanf(fp, "%2s", magic) != 1 || strcmp(magic, "P6") != 0) {
        fprintf(stderr, "Error: %s no es un archivo PPM P6 válido\n", filename);
        fclose(fp);
        return NULL;
    }

    ppm_skip_comments(fp);
    int width, height, max_color;
    if (fscanf(fp, "%d", &width) != 1) {
        fclose(fp);
        return NULL;
    }

    ppm_skip_comments(fp);
    if (fscanf(fp, "%d", &height) != 1) {
        fclose(fp);
        return NULL;
    }

    ppm_skip_comments(fp);
    if (fscanf(fp, "%d", &max_color) != 1) {
        fclose(fp);
        return NULL;
    }

    // Consume trailing single whitespace char after max_color
    fgetc(fp);

    ImagePPM *img = ppm_create(width, height);
    if (!img) {
        fclose(fp);
        return NULL;
    }
    img->max_color = max_color;

    size_t total_pixels = (size_t)width * height;
    size_t read_count = fread(img->data, sizeof(PixelRGB), total_pixels, fp);
    if (read_count != total_pixels) {
        fprintf(stderr, "Advertencia: Lectura incompleta de pixeles (%zu de %zu)\n", read_count, total_pixels);
    }

    fclose(fp);
    return img;
}

// Write PPM (P6 binary format)
static inline int ppm_write(const char *filename, const ImagePPM *img) {
    if (!img || !img->data) return 0;
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error guardando archivo PPM: %s\n", filename);
        return 0;
    }

    fprintf(fp, "P6\n# Generado por TP Final CA - Cartoon Generator\n%d %d\n%d\n",
            img->width, img->height, img->max_color);

    size_t total_pixels = (size_t)img->width * img->height;
    size_t written = fwrite(img->data, sizeof(PixelRGB), total_pixels, fp);
    fclose(fp);

    return (written == total_pixels);
}

// Generate synthetic RGB test pattern (geometric shapes, smooth gradients, high frequency textures)
static inline ImagePPM* ppm_generate_synthetic(int width, int height) {
    ImagePPM *img = ppm_create(width, height);
    if (!img) return NULL;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;

            // Base smooth RGB gradients
            int r = (x * 255) / (width > 1 ? width - 1 : 1);
            int g = (y * 255) / (height > 1 ? height - 1 : 1);
            int b = ((x + y) * 255) / ((width + height > 2) ? (width + height - 2) : 1);

            // Add geometric circles/blocks for sharp edge detection
            int cx = width / 2;
            int cy = height / 2;
            int dx = x - cx;
            int dy = y - cy;
            int dist_sq = dx * dx + dy * dy;
            int rad = (width < height ? width : height) / 3;

            if (dist_sq < rad * rad) {
                // Interior circle: contrasting bright colors
                r = 255 - r;
                g = (g + 128) % 256;
                b = 200;
            }

            // Diagonal stripes
            if (((x / 40) + (y / 40)) % 2 == 0) {
                r = (r + 40 > 255) ? 255 : r + 40;
                b = (b > 30) ? b - 30 : 0;
            }

            img->data[idx].r = (uint8_t)r;
            img->data[idx].g = (uint8_t)g;
            img->data[idx].b = (uint8_t)b;
        }
    }

    return img;
}

#endif // PPM_UTILS_H
