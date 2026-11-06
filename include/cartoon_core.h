#ifndef CARTOON_CORE_H
#define CARTOON_CORE_H

#include "ppm_utils.h"
#include <math.h>

// Helper clamp macro
static inline uint8_t clamp_u8(int val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return (uint8_t)val;
}

// Posterization function for a single 8-bit channel value
// Divides [0, 255] into K discrete levels and maps to level centers.
// For K=3: intervals [0, 85], [86, 170], [171, 255] mapped to representative values (e.g. 42, 127, 212)
// Formula: index = (val * K) / 256; mapped_value = (index * 255) / (K - 1)
static inline uint8_t quantize_channel(uint8_t val, int k_levels) {
    if (k_levels <= 1) return val;
    int bucket = (val * k_levels) / 256;
    if (bucket >= k_levels) bucket = k_levels - 1;
    int mapped = (bucket * 255) / (k_levels - 1);
    return (uint8_t)mapped;
}

// Convert RGB pixel to grayscale intensity using standard ITU-R luminance weights
static inline uint8_t rgb_to_gray(PixelRGB p) {
    // 0.299 * R + 0.587 * G + 0.114 * B (scaled by 1000 for integer arithmetic)
    int gray = (299 * (int)p.r + 587 * (int)p.g + 114 * (int)p.b) / 1000;
    return (uint8_t)gray;
}

// 1.a) Low-pass Mean Filter (Box Blur)
// Supports filter_size 3 (radius 1) or 5 (radius 2)
// Uses black neutral border (0,0,0) when coordinates fall outside [0, height) x [0, width)
static inline void apply_mean_filter_rgb(
    const PixelRGB *src, 
    PixelRGB *dst, 
    int width, 
    int height, 
    int filter_size) 
{
    int radius = filter_size / 2;
    int num_pixels_window = filter_size * filter_size;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int sum_r = 0, sum_g = 0, sum_b = 0;

            for (int dy = -radius; dy <= radius; ++dy) {
                int ny = y + dy;
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx;
                    // Neutral black border if out of bounds
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

// 1.b) Sobel Edge Highlighting + Thresholding
// Applied to smoothed grayscale version of the image
// Mask 3x3 Sobel:
// Gx = [-1 0 +1; -2 0 +2; -1 0 +1]
// Gy = [-1 -2 -1;  0  0  0; +1 +2 +1]
// Returns edge map: edge_mask[idx] is 255 if edge detected, 0 otherwise.
static inline void apply_sobel_edge_detection(
    const PixelRGB *src, 
    uint8_t *edge_out, 
    int width, 
    int height, 
    int threshold) 
{
    // Convert source to grayscale first for Sobel gradient calculation
    // Radius 1 border behavior: neutral 0 intensity for outer pixels
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int gx = 0;
            int gy = 0;

            // Convolution 3x3
            for (int dy = -1; dy <= 1; ++dy) {
                int ny = y + dy;
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx;
                    int intensity = 0; // neutral black (0)
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        intensity = rgb_to_gray(src[ny * width + nx]);
                    }

                    // Sobel weights
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

            // Approximate or exact gradient magnitude: sqrt(gx^2 + gy^2)
            int mag = (int)sqrt((double)(gx * gx + gy * gy));
            int idx = y * width + x;

            // Thresholding: Mark 255 if edge, 0 otherwise
            edge_out[idx] = (mag >= threshold) ? 255 : 0;
        }
    }
}

// 1.c) Posterization of RGB Image
static inline void apply_posterization(
    const PixelRGB *src, 
    PixelRGB *dst, 
    int total_pixels, 
    int k_levels) 
{
    for (int i = 0; i < total_pixels; ++i) {
        dst[i].r = quantize_channel(src[i].r, k_levels);
        dst[i].g = quantize_channel(src[i].g, k_levels);
        dst[i].b = quantize_channel(src[i].b, k_levels);
    }
}

// 1.d) Saturated Addition (Saturating Clamp 0-255)
// Combines posterized colors with edge detection.
// In classic cartooning:
// dst = clamp(posterized + edge)
static inline void apply_saturated_addition(
    const PixelRGB *posterized, 
    const uint8_t *edges, 
    PixelRGB *dst, 
    int total_pixels) 
{
    for (int i = 0; i < total_pixels; ++i) {
        uint8_t edge_val = edges[i];
        dst[i].r = clamp_u8((int)posterized[i].r + edge_val);
        dst[i].g = clamp_u8((int)posterized[i].g + edge_val);
        dst[i].b = clamp_u8((int)posterized[i].b + edge_val);
    }
}

#endif // CARTOON_CORE_H
