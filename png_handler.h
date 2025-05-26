#ifndef PNG_HANDLER_H
#define PNG_HANDLER_H

#include "image.h"
#include <png.h> // For png_infop

// Reads a PNG file into an Image struct
Image* read_png_file(const char *filename);

// Writes an Image struct to a PNG file
// Returns 1 on success, 0 on failure
int write_png_file(const char *filename, Image *img);

// Prints information about the PNG file
void print_png_info(const char *filename);

#endif // PNG_HANDLER_H