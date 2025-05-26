#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "image.h"
#include "utils.h" // For RGBColor

// (1) Draw triangle
void op_draw_triangle(Image *img, int x1, int y1, int x2, int y2, int x3, int y3,
                      int thickness, RGBColor line_color, int fill, RGBColor fill_color_rgb);

// (2) Find biggest rectangle of old_color and repaint with new_color
void op_biggest_rect(Image *img, RGBColor old_color_rgb, RGBColor new_color_rgb);

// (3) Create collage
// Returns a NEW image which is the collage. Original image is not modified.
// Caller is responsible for freeing the returned image.
Image* op_collage(const Image *original_img, int num_x, int num_y);

#endif // OPERATIONS_H