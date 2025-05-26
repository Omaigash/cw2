#include "image.h"
#include <stdlib.h>
#include <stdio.h> // For error messages
#include <string.h> // For memset
#include <math.h>   // For abs, sqrt in more advanced line/triangle drawing

Image* createImage(int width, int height) {
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Error: Image dimensions must be positive.\n");
        return NULL;
    }
    Image *img = (Image*)malloc(sizeof(Image));
    if (!img) {
        perror("Error allocating Image struct");
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->pixels = (Pixel*)malloc(width * height * sizeof(Pixel));
    if (!img->pixels) {
        perror("Error allocating pixel data");
        free(img);
        return NULL;
    }
    // Initialize to transparent black
    memset(img->pixels, 0, width * height * sizeof(Pixel));
    return img;
}

void freeImage(Image *img) {
    if (img) {
        free(img->pixels);
        free(img);
    }
}

Pixel getPixel(const Image *img, int x, int y) {
    if (!img || !img->pixels || x < 0 || x >= img->width || y < 0 || y >= img->height) {
        Pixel black_transparent = {0, 0, 0, 0}; // Default
        return black_transparent;
    }
    return img->pixels[y * img->width + x];
}

void setPixel(Image *img, int x, int y, Pixel color) {
    if (!img || !img->pixels || x < 0 || x >= img->width || y < 0 || y >= img->height) {
        return; // Out of bounds
    }
    img->pixels[y * img->width + x] = color;
}

// Basic Bresenham's line algorithm
void drawLineInternal(Image *img, int x0, int y0, int x1, int y1, Pixel color) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;  // error value e_xy
    int e2;

    while (1) {
        setPixel(img, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { // e_xy+e_x > 0
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) { // e_xy+e_y < 0
            err += dx;
            y0 += sy;
        }
    }
}

// Thick line drawing by drawing multiple parallel lines (simplified)
// A more robust method would involve drawing filled polygons or circles at each step.
void drawLine(Image *img, int x0, int y0, int x1, int y1, Pixel color, int thickness) {
    if (thickness <= 0) thickness = 1;

    if (thickness == 1) {
        drawLineInternal(img, x0, y0, x1, y1, color);
        return;
    }

    // For thickness > 1, this is a simplification.
    // A proper thick line requires more complex geometry (e.g., drawing a rectangle along the line path).
    // This simplified version will draw several parallel lines, which might look gappy for non-axis-aligned lines.
    // For coursework, this might be acceptable, or you might need a more advanced algorithm.
    int dx = x1 - x0;
    int dy = y1 - y0;

    for (int i = -thickness / 2; i <= thickness / 2; ++i) {
        if (abs(dx) > abs(dy)) { // More horizontal line
            drawLineInternal(img, x0, y0 + i, x1, y1 + i, color);
        } else { // More vertical line
            drawLineInternal(img, x0 + i, y0, x1 + i, y1, color);
        }
    }
     // One more pass to ensure center is drawn if thickness is even
    if (thickness % 2 == 0 && thickness > 0) {
         if (abs(dx) > abs(dy)) {
            drawLineInternal(img, x0, y0 + thickness / 2, x1, y1 + thickness / 2, color);
        } else {
            drawLineInternal(img, x0 + thickness / 2, y0, x1 + thickness / 2, y1, color);
        }
    }
}


void fillRectangle(Image *img, int x, int y, int w, int h, Pixel color) {
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            setPixel(img, x + i, y + j, color);
        }
    }
}