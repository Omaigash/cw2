#ifndef IMAGE_H
#define IMAGE_H

// RGBA Pixel structure
typedef struct {
    unsigned char r, g, b, a;
} Pixel;

// Image structure
typedef struct {
    int width;
    int height;
    Pixel *pixels; // 1D array of pixels (row-major order)
} Image;

// Function to create a new image
Image* createImage(int width, int height);

// Function to free image memory
void freeImage(Image *img);

// Get pixel at (x, y)
// Returns a default (black, transparent) pixel if out of bounds
Pixel getPixel(const Image *img, int x, int y);

// Set pixel at (x, y)
// Does nothing if out of bounds
void setPixel(Image *img, int x, int y, Pixel color);

// Basic line drawing (e.g., Bresenham's algorithm)
void drawLine(Image *img, int x0, int y0, int x1, int y1, Pixel color, int thickness);

// Fill a rectangle
void fillRectangle(Image *img, int x, int y, int w, int h, Pixel color);

#endif // IMAGE_H