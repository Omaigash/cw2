#ifndef UTILS_H
#define UTILS_H

#include "image.h" // For Pixel struct

// Structure to hold RGB color
typedef struct {
    unsigned char r, g, b;
} RGBColor;

// Parses a color string "R.G.B" into an RGBColor struct
// Returns 1 on success, 0 on failure
int parse_color_string(const char *s, RGBColor *color);

// Parses a points string "x1.y1.x2.y2.x3.y3" for triangle
// Returns 1 on success, 0 on failure
int parse_triangle_points(const char *s, int *x1, int *y1, int *x2, int *y2, int *x3, int *y3);

void print_help(const char *prog_name);

#endif // UTILS_H