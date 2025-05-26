#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For atoi

int parse_color_string(const char *s, RGBColor *color) {
    if (sscanf(s, "%hhu.%hhu.%hhu", &color->r, &color->g, &color->b) == 3) {
        return 1;
    }
    fprintf(stderr, "Error: Invalid color format. Expected RRR.GGG.BBB (e.g., 255.0.0)\n");
    return 0;
}

int parse_triangle_points(const char *s, int *x1, int *y1, int *x2, int *y2, int *x3, int *y3) {
    if (sscanf(s, "%d.%d.%d.%d.%d.%d", x1, y1, x2, y2, x3, y3) == 6) {
        return 1;
    }
    fprintf(stderr, "Error: Invalid points format for triangle. Expected x1.y1.x2.y2.x3.y3\n");
    return 0;
}

void print_help(const char *prog_name) {
    printf("Usage: %s [options] [input_file.png]\n", prog_name);
    printf("Processes PNG images. If no input_file is given, it might operate on a default or expect one via an option.\n");
    printf("If no operation is specified, this help message is shown.\n\n");
    printf("Options:\n");
    printf("  -h, --help                 Show this help message and exit.\n");
    printf("  -o, --output <filename>    Specify output PNG file (default: out.png).\n");
    printf("      --info                 Print detailed information about the input PNG file and exit.\n\n");
    printf("Operations (only one can be performed per run):\n");
    printf("  --triangle                 Draw a triangle.\n");
    printf("    --points <x1.y1.x2.y2.x3.y3> Coordinates of the triangle vertices (required for --triangle).\n");
    printf("    --thickness <num>        Line thickness (must be > 0, default: 1).\n");
    printf("    --color <R.G.B>          Line color (e.g., 255.0.0 for red, required for --triangle).\n");
    printf("    --fill                   Fill the triangle (optional).\n");
    printf("    --fill_color <R.G.B>     Color to fill the triangle with (e.g., 0.255.0 for green, required if --fill is used).\n\n");
    printf("  --biggest_rect             Find the largest rectangle of a specific color and repaint it.\n");
    printf("    --old_color <R.G.B>      Color of the rectangle to find (required for --biggest_rect).\n");
    printf("    --new_color <R.G.B>      Color to repaint the found rectangle with (required for --biggest_rect).\n\n");
    printf("  --collage                  Create a collage from the input image.\n");
    printf("    --number_x <num>         Number of repetitions along the X-axis (must be > 0, required for --collage).\n");
    printf("    --number_y <num>         Number of repetitions along the Y-axis (must be > 0, required for --collage).\n\n");
    printf("Example:\n");
    printf("  %s --triangle --points 0.0.100.0.50.80 --color 255.0.0 --thickness 2 input.png -o output.png\n", prog_name);
    printf("  %s --info input.png\n", prog_name);
}