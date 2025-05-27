#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <png.h>
#include <math.h> 

#define ERROR_SUCCESS 0
#define ERROR_ARG 40
#define ERROR_FILE 41
#define ERROR_OPERATION_FLAG 42 
#define ERROR_MEMORY 43
#define ERROR_PNG_FORMAT 44

struct Png {
    int width, height;
    png_byte color_type;
    png_byte bit_depth;
    png_structp png_ptr_read; 
    png_infop info_ptr_read;  
    int number_of_passes;     
    png_bytep *row_pointers;
    int status; 
    int original_height_for_row_pointers; 
};

typedef struct {
    unsigned char r, g, b;
} Rgb;

typedef struct {
    int x, y;
} Point;

void read_png_file(const char *filename, struct Png *image);
void write_png_file(const char *filename, struct Png *image, Rgb **rgb_data); 
void print_png_info(struct Png *image);
Rgb** png_data_to_rgb_array(struct Png *image);
void free_rgb_array(Rgb **rgb_data, int height);
void free_png_read_resources(struct Png *image); 
void print_help();
int parse_color_string(const char* optarg_str, Rgb* color_struct);
int parse_points_string(const char* optarg_str, Point* p1, Point* p2, Point* p3);

void draw_line_thick(Rgb **pixels, int W, int H, Point p1, Point p2, Rgb color, int thickness);
void fill_triangle_half_space(Rgb **pixels, int W, int H, Point v0, Point v1, Point v2, Rgb color);
void operation_draw_triangle(Rgb **pixels, int W, int H, Point p1, Point p2, Point p3, int thickness, Rgb line_color, bool fill, Rgb fill_color);
void operation_find_recolor_biggest_rect(Rgb **pixels, int W, int H, Rgb old_color, Rgb new_color);
Rgb** operation_create_collage(Rgb **original_pixels, int orig_W, int orig_H, int N_x, int M_y, int* new_W, int* new_H);


void set_pixel_safe(Rgb **pixels, int W, int H, int x, int y, Rgb color) {
    if (x >= 0 && x < W && y >= 0 && y < H) {
        pixels[y][x] = color;
    }
}

void draw_thick_dot(Rgb **pixels, int W, int H, int cx, int cy, int thickness, Rgb color) {
    if (thickness <= 0) return;
    
    for (int dy = 0; dy < thickness; ++dy) {
        for (int dx = 0; dx < thickness; ++dx) {
            int current_x = cx + dx - (thickness -1 )/2;
            int current_y = cy + dy - (thickness -1)/2;
            set_pixel_safe(pixels, W, H, current_x, current_y, color);
        }
    }
}


void draw_line_thick(Rgb **pixels, int W, int H, Point p1, Point p2, Rgb color, int thickness) {
    int x1 = p1.x, y1 = p1.y;
    int x2 = p2.x, y2 = p2.y;

    int dx_abs = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy_abs = abs(y2 - y1); 
    int sy = y1 < y2 ? 1 : -1;
    int err = (dx_abs > dy_abs ? dx_abs : -dy_abs) / 2;
    int e2;

    while (true) {
        draw_thick_dot(pixels, W, H, x1, y1, thickness, color); 
        if (x1 == x2 && y1 == y2) break;
        e2 = err;
        if (e2 > -dx_abs) { err -= dy_abs; x1 += sx; }
        if (e2 <  dy_abs) { err += dx_abs; y1 += sy; }
    }
}

static inline int edge_function(Point a, Point b, Point c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

void fill_triangle_half_space(Rgb **pixels, int W, int H, Point v0, Point v1, Point v2, Rgb color) {
    int minX = v0.x < v1.x ? (v0.x < v2.x ? v0.x : v2.x) : (v1.x < v2.x ? v1.x : v2.x);
    int minY = v0.y < v1.y ? (v0.y < v2.y ? v0.y : v2.y) : (v1.y < v2.y ? v1.y : v2.y);
    int maxX = v0.x > v1.x ? (v0.x > v2.x ? v0.x : v2.x) : (v1.x > v2.x ? v1.x : v2.x);
    int maxY = v0.y > v1.y ? (v0.y > v2.y ? v0.y : v2.y) : (v1.y > v2.y ? v1.y : v2.y);

    minX = minX < 0 ? 0 : minX;
    minY = minY < 0 ? 0 : minY;
    maxX = maxX >= W ? W - 1 : maxX;
    maxY = maxY >= H ? H - 1 : maxY;
    
    Point tv0 = v0, tv1 = v1, tv2 = v2;
    if (edge_function(v0,v1,v2) < 0) { 
        tv1 = v2;
        tv2 = v1;
    }

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Point p = {x, y};
            int w0 = edge_function(tv1, tv2, p);
            int w1 = edge_function(tv2, tv0, p);
            int w2 = edge_function(tv0, tv1, p);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                set_pixel_safe(pixels, W, H, x, y, color);
            }
        }
    }
}


void operation_draw_triangle(Rgb **pixels, int W, int H, Point p1, Point p2, Point p3, int thickness, Rgb line_color, bool fill, Rgb fill_color) {
    if (fill) {
        fill_triangle_half_space(pixels, W, H, p1, p2, p3, fill_color);
    }
    if (thickness > 0) {
        draw_line_thick(pixels, W, H, p1, p2, line_color, thickness);
        draw_line_thick(pixels, W, H, p2, p3, line_color, thickness);
        draw_line_thick(pixels, W, H, p3, p1, line_color, thickness);
    }
}

void operation_find_recolor_biggest_rect(Rgb **pixels, int W, int H, Rgb old_color, Rgb new_color) {
    if (W == 0 || H == 0) return;

    int *height_hist = (int*)calloc(W, sizeof(int)); 
    if (!height_hist) {fprintf(stderr, "Memory allocation failed for histogram height_hist\n"); return;}
    
    int max_area = 0;
    Point best_top_left = {0,0};
    Point best_bottom_right = {-1,-1}; 

    for (int r = 0; r < H; ++r) {
        for (int c = 0; c < W; ++c) {
            if (pixels[r][c].r == old_color.r && pixels[r][c].g == old_color.g && pixels[r][c].b == old_color.b) {
                height_hist[c]++;
            } else {
                height_hist[c] = 0;
            }
        }

        int *stack = (int*)malloc(sizeof(int) * (W + 1)); 
        if(!stack) {fprintf(stderr, "Memory allocation failed for stack\n"); free(height_hist); return;}
        int top = -1; 

        for (int c_hist = 0; c_hist <= W; ++c_hist) {
            int current_h_bar = (c_hist == W) ? 0 : height_hist[c_hist]; 
            while (top != -1 && height_hist[stack[top]] >= current_h_bar) {
                int h_bar = height_hist[stack[top--]];
                int w_bar = (top == -1) ? c_hist : (c_hist - stack[top] - 1);
                if (h_bar * w_bar > max_area) {
                    max_area = h_bar * w_bar;
                    best_top_left.x = (top == -1) ? 0 : stack[top] + 1;
                    best_top_left.y = r - h_bar + 1;
                    best_bottom_right.x = c_hist - 1;
                    best_bottom_right.y = r;
                }
            }
            stack[++top] = c_hist;
        }
        free(stack);
    }
    free(height_hist);

    if (max_area > 0) {
        for (int y = best_top_left.y; y <= best_bottom_right.y; ++y) {
            for (int x = best_top_left.x; x <= best_bottom_right.x; ++x) {
                set_pixel_safe(pixels, W, H, x, y, new_color);
            }
        }
    }
}


Rgb** operation_create_collage(Rgb **original_pixels, int orig_W, int orig_H, int N_x, int M_y, int* new_W_ptr, int* new_H_ptr) {
    *new_W_ptr = orig_W * N_x;
    *new_H_ptr = orig_H * M_y;

    if (*new_W_ptr == 0 || *new_H_ptr == 0) { 
        *new_W_ptr = 0; *new_H_ptr = 0; return NULL;
    }

    Rgb** collage_pixels = (Rgb**)malloc(sizeof(Rgb*) * (*new_H_ptr));
    if (!collage_pixels) {
        fprintf(stderr, "Memory for collage rows failed\n");
        *new_W_ptr = 0; *new_H_ptr = 0; return NULL;
    }

    for (int y = 0; y < *new_H_ptr; ++y) {
        collage_pixels[y] = (Rgb*)malloc(sizeof(Rgb) * (*new_W_ptr));
        if (!collage_pixels[y]) {
            fprintf(stderr, "Memory for collage row %d failed\n", y);
            for (int k=0; k<y; ++k) free(collage_pixels[k]);
            free(collage_pixels);
            *new_W_ptr = 0; *new_H_ptr = 0; return NULL;
        }
    }

    for (int tile_m = 0; tile_m < M_y; ++tile_m) { 
        for (int tile_n = 0; tile_n < N_x; ++tile_n) { 
            for (int y_in_tile = 0; y_in_tile < orig_H; ++y_in_tile) {
                for (int x_in_tile = 0; x_in_tile < orig_W; ++x_in_tile) {
                    int target_y = tile_m * orig_H + y_in_tile;
                    int target_x = tile_n * orig_W + x_in_tile;
                    collage_pixels[target_y][target_x] = original_pixels[y_in_tile][x_in_tile];
                }
            }
        }
    }
    return collage_pixels;
}


void read_png_file(const char *filename, struct Png *image) {
    image->status = ERROR_SUCCESS;
    image->row_pointers = NULL;
    image->png_ptr_read = NULL;
    image->info_ptr_read = NULL;
    image->original_height_for_row_pointers = 0; 

    png_byte header[8];
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s for reading.\n", filename);
        image->status = ERROR_FILE;
        return;
    }

    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "Error: %s is not a valid PNG file.\n", filename);
        image->status = ERROR_PNG_FORMAT;
        fclose(fp);
        return;
    }

    image->png_ptr_read = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!image->png_ptr_read) {
        fprintf(stderr, "Error: png_create_read_struct failed.\n");
        image->status = ERROR_MEMORY;
        fclose(fp);
        return;
    }

    image->info_ptr_read = png_create_info_struct(image->png_ptr_read);
    if (!image->info_ptr_read) {
        fprintf(stderr, "Error: png_create_info_struct failed.\n");
        image->status = ERROR_MEMORY;
        png_destroy_read_struct(&image->png_ptr_read, NULL, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(image->png_ptr_read))) {
        fprintf(stderr, "Error: libpng error during init_io.\n");
        image->status = ERROR_PNG_FORMAT;
        png_destroy_read_struct(&image->png_ptr_read, &image->info_ptr_read, NULL);
        fclose(fp);
        return;
    }

    png_init_io(image->png_ptr_read, fp);
    png_set_sig_bytes(image->png_ptr_read, 8);
    png_read_info(image->png_ptr_read, image->info_ptr_read);
    
    image->original_height_for_row_pointers = png_get_image_height(image->png_ptr_read, image->info_ptr_read);
    image->width = png_get_image_width(image->png_ptr_read, image->info_ptr_read);
    image->height = image->original_height_for_row_pointers; 
    image->color_type = png_get_color_type(image->png_ptr_read, image->info_ptr_read);
    image->bit_depth = png_get_bit_depth(image->png_ptr_read, image->info_ptr_read);
    image->number_of_passes = png_set_interlace_handling(image->png_ptr_read);
    
    if (image->bit_depth == 16)
        png_set_strip_16(image->png_ptr_read);
    if (image->color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(image->png_ptr_read);
    if (image->color_type == PNG_COLOR_TYPE_GRAY && image->bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(image->png_ptr_read);
    if (png_get_valid(image->png_ptr_read, image->info_ptr_read, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(image->png_ptr_read);
    if (image->color_type == PNG_COLOR_TYPE_GRAY || image->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(image->png_ptr_read);

    png_read_update_info(image->png_ptr_read, image->info_ptr_read);
    
    image->width = png_get_image_width(image->png_ptr_read, image->info_ptr_read);
    image->height = png_get_image_height(image->png_ptr_read, image->info_ptr_read); 
    if(image->original_height_for_row_pointers != image->height){
        image->original_height_for_row_pointers = image->height; 
    }

    image->color_type = png_get_color_type(image->png_ptr_read, image->info_ptr_read);
    image->bit_depth = png_get_bit_depth(image->png_ptr_read, image->info_ptr_read);

    if (image->color_type != PNG_COLOR_TYPE_RGB && image->color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
        fprintf(stderr, "Error: Only RGB and RGBA color types are supported by this program after conversion (got %d).\n", image->color_type);
        image->status = ERROR_PNG_FORMAT; 
        png_destroy_read_struct(&image->png_ptr_read, &image->info_ptr_read, NULL);
        fclose(fp);
        return;
    }

    if(image->original_height_for_row_pointers == 0 && image->height > 0) { 
        image->original_height_for_row_pointers = image->height;
    }

    image->row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * image->original_height_for_row_pointers); 
    if (!image->row_pointers && image->original_height_for_row_pointers > 0) { 
        fprintf(stderr, "Error: Malloc for row_pointers failed.\n");
        image->status = ERROR_MEMORY;
        png_destroy_read_struct(&image->png_ptr_read, &image->info_ptr_read, NULL);
        fclose(fp);
        return;
    } 

    for (int y = 0; y < image->original_height_for_row_pointers; y++) { 
        size_t row_bytes_alloc = png_get_rowbytes(image->png_ptr_read, image->info_ptr_read);
        if (row_bytes_alloc == 0) {
             fprintf(stderr, "Error: Calculated row bytes is zero for allocation at row %d.\n", y);
             image->status = ERROR_PNG_FORMAT;
             for(int k=0; k<y; ++k) { if(image->row_pointers[k]) free(image->row_pointers[k]); }
             if(image->row_pointers) { free(image->row_pointers); image->row_pointers = NULL; }
             png_destroy_read_struct(&image->png_ptr_read, &image->info_ptr_read, NULL);
             fclose(fp);
             return;
        }
        image->row_pointers[y] = (png_byte*)malloc(row_bytes_alloc);
        if (!image->row_pointers[y]) {
            fprintf(stderr, "Error: Malloc for row %d failed.\n", y);
            image->status = ERROR_MEMORY;
            for(int k=0; k<y; ++k) { if(image->row_pointers[k]) free(image->row_pointers[k]); }
            if(image->row_pointers) {
                free(image->row_pointers); 
                image->row_pointers = NULL; 
            }
            png_destroy_read_struct(&image->png_ptr_read, &image->info_ptr_read, NULL);
            fclose(fp);
            return;
        }
    }

    if (setjmp(png_jmpbuf(image->png_ptr_read))) {
        fprintf(stderr, "Error: libpng error during read_image.\n");
        image->status = ERROR_PNG_FORMAT;
        png_destroy_read_struct(&image->png_ptr_read, &image->info_ptr_read, NULL); 
        fclose(fp);
        return;
    }
    if(image->original_height_for_row_pointers > 0) { 
        png_read_image(image->png_ptr_read, image->row_pointers);
    }
    fclose(fp);
}

void write_png_file(const char *filename, struct Png *image_props, Rgb **rgb_data) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s for writing.\n", filename);
        image_props->status = ERROR_FILE;
        return;
    }

    png_structp png_ptr_write = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr_write) {
        fprintf(stderr, "Error: png_create_write_struct failed.\n");
        image_props->status = ERROR_MEMORY;
        fclose(fp);
        return;
    }

    png_infop info_ptr_write = png_create_info_struct(png_ptr_write);
    if (!info_ptr_write) {
        fprintf(stderr, "Error: png_create_info_struct failed.\n");
        image_props->status = ERROR_MEMORY;
        png_destroy_write_struct(&png_ptr_write, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr_write))) {
        fprintf(stderr, "Error: libpng error during write init_io/IHDR.\n");
        image_props->status = ERROR_PNG_FORMAT;
        png_destroy_write_struct(&png_ptr_write, &info_ptr_write);
        fclose(fp);
        return;
    }

    png_init_io(png_ptr_write, fp);
    png_set_IHDR(png_ptr_write, info_ptr_write, image_props->width, image_props->height,
                 image_props->bit_depth, image_props->color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr_write, info_ptr_write);

    if(image_props->height == 0 || image_props->width == 0){ 
        png_write_end(png_ptr_write, NULL);
        png_destroy_write_struct(&png_ptr_write, &info_ptr_write);
        fclose(fp);
        return;
    }

    png_bytep *write_row_pointers = NULL;
    if (image_props->height > 0) {
        write_row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * image_props->height);
        if(!write_row_pointers){
            fprintf(stderr, "Error: Malloc for write_row_pointers failed.\n");
            image_props->status = ERROR_MEMORY;
            png_destroy_write_struct(&png_ptr_write, &info_ptr_write);
            fclose(fp);
            return;
        }
    }
    
    int channels = (image_props->color_type == PNG_COLOR_TYPE_RGB_ALPHA) ? 4 : 3;
    size_t row_bytes = image_props->width * channels; 

    for (int y = 0; y < image_props->height; y++) {
        if (row_bytes == 0) {
            fprintf(stderr, "Error: Calculated row_bytes is zero for writing at row %d.\n", y);
            image_props->status = ERROR_PNG_FORMAT;
            for(int k=0; k<y; ++k) if(write_row_pointers && write_row_pointers[k]) free(write_row_pointers[k]);
            if(write_row_pointers) free(write_row_pointers);
            png_destroy_write_struct(&png_ptr_write, &info_ptr_write);
            fclose(fp);
            return;
        }
        write_row_pointers[y] = (png_byte*)malloc(row_bytes);
        if(!write_row_pointers[y]){
            fprintf(stderr, "Error: Malloc for write_row_pointers[%d] failed.\n", y);
            image_props->status = ERROR_MEMORY;
            for(int k=0; k<y; ++k) if(write_row_pointers[k]) free(write_row_pointers[k]);
            if(write_row_pointers)free(write_row_pointers);
            png_destroy_write_struct(&png_ptr_write, &info_ptr_write);
            fclose(fp);
            return;
        }
        for (int x = 0; x < image_props->width; x++) {
            png_bytep px = &(write_row_pointers[y][x * channels]);
            px[0] = rgb_data[y][x].r;
            px[1] = rgb_data[y][x].g;
            px[2] = rgb_data[y][x].b;
            if (channels == 4) {
                px[3] = 255; 
            }
        }
    }

    if (setjmp(png_jmpbuf(png_ptr_write))) {
        fprintf(stderr, "Error: libpng error during png_write_image.\n");
        image_props->status = ERROR_PNG_FORMAT;
        if (write_row_pointers) {
            for (int y_clean = 0; y_clean < image_props->height; y_clean++) if(write_row_pointers[y_clean]) free(write_row_pointers[y_clean]); 
            free(write_row_pointers);
        }
        png_destroy_write_struct(&png_ptr_write, &info_ptr_write);
        fclose(fp);
        return;
    }
    if (image_props->height > 0 && write_row_pointers) {
        png_write_image(png_ptr_write, write_row_pointers);
    }
    png_write_end(png_ptr_write, NULL);

    if (write_row_pointers) {
        for (int y = 0; y < image_props->height; y++) {
            if(write_row_pointers[y]) free(write_row_pointers[y]);
        }
        free(write_row_pointers);
    }
    png_destroy_write_struct(&png_ptr_write, &info_ptr_write);
    fclose(fp);
}


void print_png_info(struct Png *image) {
    if (!image || (image->status != ERROR_SUCCESS && image->width == 0 && image->height == 0 && image->bit_depth == 0)) {
        fprintf(stderr, "Cannot display info due to previous error or invalid image data structure.\n");
        return;
    }
    printf("Width: %d\n", image->width);
    printf("Height: %d\n", image->height); 
    printf("Bit depth: %d\n", image->bit_depth);
    const char* ct_str;
    switch(image->color_type){
        case PNG_COLOR_TYPE_GRAY: ct_str = "Grayscale"; break;
        case PNG_COLOR_TYPE_GRAY_ALPHA: ct_str = "Grayscale with Alpha"; break;
        case PNG_COLOR_TYPE_PALETTE: ct_str = "Palette"; break;
        case PNG_COLOR_TYPE_RGB: ct_str = "RGB"; break;
        case PNG_COLOR_TYPE_RGB_ALPHA: ct_str = "RGBA"; break;
        default: ct_str = "Unknown"; break;
    }
    printf("Color type: %s (%d)\n", ct_str, image->color_type);
    if (image->png_ptr_read && image->info_ptr_read) { 
        printf("Number of passes (interlace): %d\n", image->number_of_passes);
    } else {
        printf("Number of passes (interlace): N/A (info not fully read)\n");
    }
}

Rgb** png_data_to_rgb_array(struct Png *image) {
    if (!image || !image->row_pointers || image->status != ERROR_SUCCESS || image->height == 0 || image->width == 0) {
        return NULL; 
    }
    
    Rgb **arr = (Rgb**)malloc(sizeof(Rgb*) * image->height);
    if (!arr) {
        fprintf(stderr, "Memory for Rgb** failed\n");
        image->status = ERROR_MEMORY;
        return NULL;
    }

    int channels = (image->color_type == PNG_COLOR_TYPE_RGB_ALPHA) ? 4 : 3;

    for (int y = 0; y < image->height; y++) {
        arr[y] = (Rgb*)malloc(sizeof(Rgb) * image->width);
        if (!arr[y]) {
            fprintf(stderr, "Memory for Rgb* row %d failed\n", y);
            for (int k = 0; k < y; k++) if(arr[k]) free(arr[k]);
            free(arr);
            image->status = ERROR_MEMORY;
            return NULL;
        }
        png_bytep row = image->row_pointers[y];
        for (int x = 0; x < image->width; x++) {
            png_bytep px = &(row[x * channels]);
            arr[y][x].r = px[0];
            arr[y][x].g = px[1];
            arr[y][x].b = px[2];
        }
    }
    return arr;
}

void free_rgb_array(Rgb **rgb_data, int height) {
    if (!rgb_data) return;
    for (int i = 0; i < height; i++) {
        if(rgb_data[i]) free(rgb_data[i]); 
    }
    free(rgb_data);
}

void free_png_read_resources(struct Png *image) {
    if (!image) return;
    if (image->row_pointers) {
        for (int y = 0; y < image->original_height_for_row_pointers; y++) { 
            if (image->row_pointers[y]) free(image->row_pointers[y]); 
        }
        free(image->row_pointers);
        image->row_pointers = NULL;
    }
    if (image->png_ptr_read || image->info_ptr_read) {
        png_destroy_read_struct(&image->png_ptr_read, &image->info_ptr_read, NULL);
        image->png_ptr_read = NULL;
        image->info_ptr_read = NULL;
    }
}

int parse_color_string(const char* optarg_str, Rgb* color_struct) {
    int r_int, g_int, b_int;
    if (!optarg_str) { 
        fprintf(stderr, "Error: Color string is NULL.\n");
        return 0;
    }
    if (sscanf(optarg_str, "%d.%d.%d", &r_int, &g_int, &b_int) != 3 ||
        r_int < 0 || r_int > 255 || g_int < 0 || g_int > 255 || b_int < 0 || b_int > 255) {
        fprintf(stderr, "Error: Incorrect color format '%s'. Expected rrr.ggg.bbb with components in 0-255.\n", optarg_str);
        return 0; 
    }
    color_struct->r = (unsigned char)r_int;
    color_struct->g = (unsigned char)g_int;
    color_struct->b = (unsigned char)b_int;
    return 1; 
}

int parse_points_string(const char* optarg_str, Point* p1, Point* p2, Point* p3) {
    if (!optarg_str) { 
        fprintf(stderr, "Error: Points string is NULL.\n");
        return 0;
    }
    if (sscanf(optarg_str, "%d.%d.%d.%d.%d.%d", &p1->x, &p1->y, &p2->x, &p2->y, &p3->x, &p3->y) != 6) {
        fprintf(stderr, "Error: Incorrect points format '%s'. Expected x1.y1.x2.y2.x3.y3.\n", optarg_str);
        return 0; 
    }
    return 1; 
}

void print_help() {
    puts("Usage: program_name [operation] [operation_args] [-i input.png] [-o output.png]");
    puts("\nOperations (only one per execution):");
    puts("  --triangle                  Draw a triangle.");
    puts("      --points <x1.y1.x2.y2.x3.y3> Vertex coordinates (required).");
    puts("      --thickness <int>       Line thickness, >0 (required).");
    puts("      --color <r.g.b>         Line color, 0-255 (required).");
    puts("      --fill                  (Optional) Flag to fill the triangle.");
    puts("      --fill_color <r.g.b>    (Optional) Fill color if --fill is used.");
    puts("\n  --biggest_rect              Find and recolor the largest rectangle of a specific color.");
    puts("      --old_color <r.g.b>     Color of the rectangle to find (required).");
    puts("      --new_color <r.g.b>     Color to repaint with (required).");
    puts("\n  --collage                   Create a collage from the input image.");
    puts("      --number_x <int>        Number of repetitions along X-axis, >0 (required).");
    puts("      --number_y <int>        Number of repetitions along Y-axis, >0 (required).");
    puts("\nOther options:");
    puts("  -i, --input <file.png>      Input PNG file name.");
    puts("  -o, --output <file.png>     Output PNG file name (default: out.png).");
    puts("      --info                  Show information about the input PNG file.");
    puts("  -h, --help                  Show this help message.");
}


int main(int argc, char *argv[]) {
    printf("Course work for option 4.19, created by Omelyash Egor\n");

    char *input_filename = NULL;
    char *output_filename = "out.png"; 

    int op_triangle_flag = 0;
    int op_biggest_rect_flag = 0;
    int op_collage_flag = 0;
    int info_flag = 0;
    int help_flag = 0;

    char* points_str = NULL; Point p1={0}, p2={0}, p3={0}; 
    int thickness = 0;
    char* line_color_str = NULL; Rgb line_color={0}; 
    int fill_flag = 0;
    char* fill_color_str = NULL; Rgb fill_color={0}; 

    char* old_color_str = NULL; Rgb old_color={0}; 
    char* new_color_str = NULL; Rgb new_color={0}; 

    int number_x = 0, number_y = 0;

    struct Png image_data;
    memset(&image_data, 0, sizeof(struct Png)); 
    image_data.status = ERROR_SUCCESS;


    const struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"input", required_argument, NULL, 'i'},
        {"output", required_argument, NULL, 'o'},
        {"info", no_argument, NULL, 256}, 

        {"triangle", no_argument, NULL, 257},
        {"points", required_argument, NULL, 'p'}, 
        {"thickness", required_argument, NULL, 't'},
        {"color", required_argument, NULL, 'c'},
        {"fill", no_argument, NULL, 258},
        {"fill_color", required_argument, NULL, 'f'},

        {"biggest_rect", no_argument, NULL, 259},
        {"old_color", required_argument, NULL, 'O'}, 
        {"new_color", required_argument, NULL, 'N'},

        {"collage", no_argument, NULL, 260},
        {"number_x", required_argument, NULL, 'x'},
        {"number_y", required_argument, NULL, 'y'},
        {0, 0, 0, 0}
    };

    int opt;

    while ((opt = getopt_long(argc, argv, "hi:o:p:t:c:f:O:N:x:y:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h': help_flag = 1; break;
            case 'i': input_filename = optarg; break;
            case 'o': output_filename = optarg; break;
            case 256: info_flag = 1; break; 

            case 257: op_triangle_flag = 1; break; 
            case 'p': points_str = optarg; break;
            case 't': thickness = atoi(optarg); break;
            case 'c': line_color_str = optarg; break;
            case 258: fill_flag = 1; break; 
            case 'f': fill_color_str = optarg; break;

            case 259: op_biggest_rect_flag = 1; break; 
            case 'O': old_color_str = optarg; break;
            case 'N': new_color_str = optarg; break;

            case 260: op_collage_flag = 1; break; 
            case 'x': number_x = atoi(optarg); break;
            case 'y': number_y = atoi(optarg); break;
            
            case '?': 
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                image_data.status = ERROR_ARG;
                goto cleanup_and_exit;
            default: 
                image_data.status = ERROR_ARG; 
                goto cleanup_and_exit;
        }
    }
    
    if (optind < argc && input_filename == NULL) {
        input_filename = argv[optind];
    }


    if (help_flag || (argc == 1 && !input_filename) ) { 
        print_help();
        image_data.status = ERROR_SUCCESS; 
        goto cleanup_and_exit;
    }

    if (!input_filename && (info_flag || op_triangle_flag || op_biggest_rect_flag || op_collage_flag)) {
         fprintf(stderr, "Error: Input file is required for this operation.\n");
         image_data.status = ERROR_FILE;
         goto cleanup_and_exit;
    }


    int num_ops = op_triangle_flag + op_biggest_rect_flag + op_collage_flag;
    if (num_ops > 1) {
        fprintf(stderr, "Error: Only one image processing operation allowed at a time.\n");
        image_data.status = ERROR_OPERATION_FLAG;
        goto cleanup_and_exit;
    }
    if (num_ops == 0 && !info_flag) { 
        fprintf(stderr, "Error: No operation specified. Use --help for options.\n");
        image_data.status = ERROR_OPERATION_FLAG;
        goto cleanup_and_exit;
    }

    if (op_triangle_flag) {
        if (!points_str || thickness <= 0 || !line_color_str) {
            fprintf(stderr, "Error: --triangle requires --points, --thickness (>0), and --color.\n");
            image_data.status = ERROR_ARG;
        }
        if (fill_flag && !fill_color_str) {
            fprintf(stderr, "Error: --fill requires --fill_color for --triangle.\n"); 
            image_data.status = ERROR_ARG;
        }
        if (image_data.status == ERROR_SUCCESS) {
             if(!parse_points_string(points_str, &p1, &p2, &p3)) image_data.status = ERROR_ARG;
             if(!parse_color_string(line_color_str, &line_color)) image_data.status = ERROR_ARG;
             if(fill_flag && !parse_color_string(fill_color_str, &fill_color)) image_data.status = ERROR_ARG;
        }
    } else if (op_biggest_rect_flag) {
        if (!old_color_str || !new_color_str) {
            fprintf(stderr, "Error: --biggest_rect requires --old_color and --new_color.\n");
            image_data.status = ERROR_ARG;
        }
         if (image_data.status == ERROR_SUCCESS) {
            if(!parse_color_string(old_color_str, &old_color)) image_data.status = ERROR_ARG;
            if(!parse_color_string(new_color_str, &new_color)) image_data.status = ERROR_ARG;
         }
    } else if (op_collage_flag) {
        if (number_x <= 0 || number_y <= 0) {
            fprintf(stderr, "Error: --collage requires --number_x > 0 and --number_y > 0.\n");
            image_data.status = ERROR_ARG;
        }
    }

    if (image_data.status != ERROR_SUCCESS) goto cleanup_and_exit;


    if (input_filename) { 
        read_png_file(input_filename, &image_data);
        if (image_data.status != ERROR_SUCCESS) {
            fprintf(stderr, "Failed to read PNG file '%s'.\n", input_filename);
            goto cleanup_and_exit;
        }
    }


    if (info_flag) {
        print_png_info(&image_data);
    } else if (num_ops > 0) { 
        Rgb **rgb_pixels = png_data_to_rgb_array(&image_data);
        if (!rgb_pixels && (image_data.width > 0 && image_data.height > 0) ) { 
            fprintf(stderr, "Failed to convert PNG to RGB array.\n");
            goto cleanup_and_exit;
        }

        int original_h_for_rgb_free = image_data.height; 

        if (op_triangle_flag) {
            if (rgb_pixels) operation_draw_triangle(rgb_pixels, image_data.width, image_data.height, p1, p2, p3, thickness, line_color, fill_flag, fill_color);
        } else if (op_biggest_rect_flag) {
            if (rgb_pixels) operation_find_recolor_biggest_rect(rgb_pixels, image_data.width, image_data.height, old_color, new_color);
        } else if (op_collage_flag) {
            int new_w, new_h;
            Rgb **collage_pixels = NULL;
            if(rgb_pixels) { 
                 collage_pixels = operation_create_collage(rgb_pixels, image_data.width, image_data.height, number_x, number_y, &new_w, &new_h);
            } else { 
                new_w = image_data.width * number_x; 
                new_h = image_data.height * number_y; 
            }

            if (rgb_pixels && !collage_pixels && (new_w > 0 || new_h > 0)) { 
                image_data.status = ERROR_MEMORY; 
                free_rgb_array(rgb_pixels, original_h_for_rgb_free); 
                goto cleanup_and_exit;
            }
            if(rgb_pixels) free_rgb_array(rgb_pixels, original_h_for_rgb_free); 
            rgb_pixels = collage_pixels;          
            image_data.width = new_w;             
            image_data.height = new_h; 
        }
        
        write_png_file(output_filename, &image_data, rgb_pixels);
        if (image_data.status != ERROR_SUCCESS) {
            fprintf(stderr, "Failed to write PNG file '%s'.\n", output_filename);
        }
        if(rgb_pixels) free_rgb_array(rgb_pixels, image_data.height); 
    }

cleanup_and_exit:
    free_png_read_resources(&image_data); 
    if (image_data.status == ERROR_SUCCESS && (num_ops > 0 || info_flag || help_flag || (argc==1 && !input_filename) )) { 
         if (num_ops > 0 && !info_flag && !help_flag) printf("Operation completed successfully. Output: %s\n", output_filename);
    } else if (image_data.status != ERROR_SUCCESS) {
         fprintf(stderr, "Program terminated with error code: %d\n", image_data.status);
    }
    return image_data.status;
}