#include "operations.h"
#include <stdlib.h> // For abs, malloc, free
#include <stdio.h>  // For debug prints
#include <limits.h> // For INT_MIN, INT_MAX

// Helper for triangle filling: sort vertices by Y
static void sort_vertices_by_y(int *x1, int *y1, int *x2, int *y2, int *x3, int *y3) {
    int tx, ty;
    if (*y1 > *y2) { tx = *x1; ty = *y1; *x1 = *x2; *y1 = *y2; *x2 = tx; *y2 = ty; }
    if (*y1 > *y3) { tx = *x1; ty = *y1; *x1 = *x3; *y1 = *y3; *x3 = tx; *y3 = ty; }
    if (*y2 > *y3) { tx = *x2; ty = *y2; *x2 = *x3; *y2 = *y3; *x3 = tx; *y3 = ty; }
}

// Helper for triangle filling: scanline fill
// This is a simplified scanline fill for a triangle.
// For robustness, edge cases and floating point precision would be needed.
static void fill_triangle_scanline(Image *img, int x1, int y1, int x2, int y2, int x3, int y3, Pixel color) {
    sort_vertices_by_y(&x1, &y1, &x2, &y2, &x3, &y3);

    // Handle flat-bottom or flat-top triangles separately or use a general approach
    // This is a very basic implementation, not covering all cases perfectly (e.g. horizontal lines)

    // Top part of triangle (y1 to y2)
    for (int y = y1; y <= y2; y++) {
        if (y1 == y2 && y2 == y3) break; // All points on same line, or single point
        if (y < 0 || y >= img->height) continue;

        int x_start, x_end;
        // Avoid division by zero
        if (y2 == y1) { // Top edge is horizontal
            x_start = (y3 == y1) ? (x1 < x3 ? x1 : x3) : x1 + (float)(x3 - x1) * (y - y1) / (y3 - y1);
        } else {
            x_start = x1 + (float)(x2 - x1) * (y - y1) / (y2 - y1);
        }
        if (y3 == y1) { // Bottom part is horizontal relative to y1 (meaning y3 related to y1)
             x_end = (y2 == y1) ? (x1 < x2 ? x1 : x2) : x1 + (float)(x2 - x1) * (y - y1) / (y2 - y1);
        } else {
             x_end = x1 + (float)(x3 - x1) * (y - y1) / (y3 - y1);
        }


        if (x_start > x_end) { int temp = x_start; x_start = x_end; x_end = temp; }
        for (int x = x_start; x <= x_end; x++) {
            setPixel(img, x, y, color);
        }
    }

    // Bottom part of triangle (y2 to y3)
    for (int y = y2 + 1; y <= y3; y++) {
        if (y2 == y3) break; // Bottom edge is horizontal or single point
        if (y < 0 || y >= img->height) continue;
        
        int x_start, x_end;
        // Avoid division by zero
        if (y2 == y1) {
            x_start = x2 + (float)(x1 - x2) * (y - y2) / (y1 - y2); // This case shouldn't happen if y2 > y1
        } else {
            x_start = x2 + (float)(x1 - x2) * (y - y2) / (y1 - y2); // This needs careful re-evaluation for this part
                                                                  // Let's use the x2,y2 to x3,y3 edge and x1,y1 to x3,y3 edge
            x_start = x2 + (y3 == y2 ? 0 : (float)(x3 - x2) * (y - y2) / (y3 - y2));
        }
        if (y3 == y1) {
             x_end = x1 + (float)(x3 - x1) * (y - y1) / (y3 - y1); // This refers to the whole side
        } else {
             x_end = x1 + (float)(x3 - x1) * (y - y1) / (y3 - y1);
        }


        if (x_start > x_end) { int temp = x_start; x_start = x_end; x_end = temp; }
        for (int x = x_start; x <= x_end; x++) {
            setPixel(img, x, y, color);
        }
    }
}


void op_draw_triangle(Image *img, int x1, int y1, int x2, int y2, int x3, int y3,
                      int thickness, RGBColor line_color_rgb, int fill, RGBColor fill_color_rgb) {
    if (!img) return;
    Pixel line_px = {line_color_rgb.r, line_color_rgb.g, line_color_rgb.b, 255};
    Pixel fill_px = {fill_color_rgb.r, fill_color_rgb.g, fill_color_rgb.b, 255};

    if (fill) {
        // A proper fill algorithm is complex (e.g., scanline rasterization).
        // For simplicity here, I'll call the placeholder.
        // You'll need to implement a robust one.
        // A common approach is to sort vertices by Y, then iterate scanlines.
        fill_triangle_scanline(img, x1, y1, x2, y2, x3, y3, fill_px);
    }

    // Draw outline
    drawLine(img, x1, y1, x2, y2, line_px, thickness);
    drawLine(img, x2, y2, x3, y3, line_px, thickness);
    drawLine(img, x3, y3, x1, y1, line_px, thickness);
}


void op_biggest_rect(Image *img, RGBColor old_color_rgb, RGBColor new_color_rgb) {
    if (!img) return;
    Pixel old_px_target = {old_color_rgb.r, old_color_rgb.g, old_color_rgb.b, 0}; // Alpha doesn't matter for comparison here
    Pixel new_px = {new_color_rgb.r, new_color_rgb.g, new_color_rgb.b, 255}; // Repaint with full alpha

    int max_area = 0;
    int best_x = -1, best_y = -1, best_w = 0, best_h = 0;

    for (int r = 0; r < img->height; ++r) {
        for (int c = 0; c < img->width; ++c) {
            Pixel current_px = getPixel(img, c, r);
            if (current_px.r == old_px_target.r && 
                current_px.g == old_px_target.g && 
                current_px.b == old_px_target.b) {
                
                // Try to expand rectangle from (c, r)
                int current_w = 1;
                while (c + current_w < img->width) {
                    Pixel next_px = getPixel(img, c + current_w, r);
                    if (next_px.r == old_px_target.r && 
                        next_px.g == old_px_target.g && 
                        next_px.b == old_px_target.b) {
                        current_w++;
                    } else {
                        break;
                    }
                }

                // Now expand downwards for this width
                int current_h = 1;
                for (int h_try = 1; r + h_try < img->height; ++h_try) {
                    int row_ok = 1;
                    for (int w_check = 0; w_check < current_w; ++w_check) {
                        Pixel check_px = getPixel(img, c + w_check, r + h_try);
                         if (check_px.r != old_px_target.r || 
                             check_px.g != old_px_target.g || 
                             check_px.b != old_px_target.b) {
                            row_ok = 0;
                            break;
                        }
                    }
                    if (row_ok) {
                        current_h++;
                    } else {
                        break;
                    }
                }
                
                if (current_w * current_h > max_area) {
                    max_area = current_w * current_h;
                    best_x = c;
                    best_y = r;
                    best_w = current_w;
                    best_h = current_h;
                }
            }
        }
    }

    if (max_area > 0) {
        // printf("Found biggest rectangle at (%d, %d) with size %dx%d, area %d\n", best_x, best_y, best_w, best_h, max_area);
        fillRectangle(img, best_x, best_y, best_w, best_h, new_px);
    } else {
        // printf("No rectangle of specified color found.\n");
    }
}


Image* op_collage(const Image *original_img, int num_x, int num_y) {
    if (!original_img || num_x <= 0 || num_y <= 0) return NULL;

    int tile_w = original_img->width;
    int tile_h = original_img->height;
    int collage_w = tile_w * num_x;
    int collage_h = tile_h * num_y;

    Image *collage_img = createImage(collage_w, collage_h);
    if (!collage_img) return NULL;

    for (int ty = 0; ty < num_y; ++ty) {    // Tile Y index
        for (int tx = 0; tx < num_x; ++tx) { // Tile X index
            int dest_start_x = tx * tile_w;
            int dest_start_y = ty * tile_h;

            for (int y = 0; y < tile_h; ++y) {
                for (int x = 0; x < tile_w; ++x) {
                    Pixel p = getPixel(original_img, x, y);
                    setPixel(collage_img, dest_start_x + x, dest_start_y + y, p);
                }
            }
        }
    }
    return collage_img;
}