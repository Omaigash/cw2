#include "png_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <png.h>

// Custom error handler for libpng
void user_error_fn(png_structp png_ptr, png_const_charp error_msg) {
    fprintf(stderr, "libpng error: %s\n", error_msg);
    // If jmp_buf is set, longjmp. Otherwise, exit.
    if (png_ptr && png_ptr->error_ptr) {
        longjmp(png_jmpbuf(png_ptr), 1);
    } else {
        exit(EXIT_FAILURE);
    }
}

// Custom warning handler for libpng
void user_warning_fn(png_structp png_ptr, png_const_charp warning_msg) {
    fprintf(stderr, "libpng warning: %s\n", warning_msg);
}


Image* read_png_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening PNG file for reading");
        return NULL;
    }

    unsigned char header[8]; // 8 is the maximum size that can be checked
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "Error: %s is not recognized as a PNG file.\n", filename);
        fclose(fp);
        return NULL;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, user_error_fn, user_warning_fn);
    if (!png_ptr) {
        fprintf(stderr, "Error: png_create_read_struct failed.\n");
        fclose(fp);
        return NULL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Error: png_create_info_struct failed.\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during libpng read init_io/read_info.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8); // We've already read 8 bytes

    png_read_info(png_ptr, info_ptr);

    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    // Ensure 8-bit depth
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    // Convert palette to RGB
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    // Expand grayscale to RGB if < 8 bits
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    // If tRNS chunk is present, convert to full alpha
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    
    // If no alpha channel, add one (RGBA)
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER); // Add OPAQUE alpha

    // Convert grayscale to RGB
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr); // Apply transformations

    // Now color_type should be RGB or RGBA
    if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGB_ALPHA) {
         fprintf(stderr, "Error: Could not convert PNG to RGBA format. Final type: %d\n", png_get_color_type(png_ptr, info_ptr));
         png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
         fclose(fp);
         return NULL;
    }


    Image *img = createImage(width, height);
    if (!img) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    png_bytep *row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        perror("Error allocating row_pointers");
        freeImage(img);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr, info_ptr));
         if (!row_pointers[y]) {
            perror("Error allocating row_pointer row");
            for(int k=0; k<y; ++k) free(row_pointers[k]);
            free(row_pointers);
            freeImage(img);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return NULL;
        }
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during libpng png_read_image.\n");
        for (int y = 0; y < height; y++) free(row_pointers[y]);
        free(row_pointers);
        freeImage(img);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    png_read_image(png_ptr, row_pointers);

    // Copy data to our Image struct
    for (int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for (int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]); // 4 bytes per pixel (RGBA)
            img->pixels[y * width + x].r = px[0];
            img->pixels[y * width + x].g = px[1];
            img->pixels[y * width + x].b = px[2];
            img->pixels[y * width + x].a = px[3];
        }
    }

    // Cleanup
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    return img;
}


int write_png_file(const char *filename, Image *img) {
    if (!img || !img->pixels) {
        fprintf(stderr, "Error: Cannot write NULL image.\n");
        return 0;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Error opening PNG file for writing");
        return 0;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, user_error_fn, user_warning_fn);
    if (!png_ptr) {
        fprintf(stderr, "Error: png_create_write_struct failed.\n");
        fclose(fp);
        return 0;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Error: png_create_info_struct failed.\n");
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return 0;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during libpng write init_io/header.\n");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 0;
    }

    png_init_io(png_ptr, fp);

    // Write header (8 bit depth, RGBA, no interlace, default compression, default filter)
    png_set_IHDR(
        png_ptr, info_ptr, img->width, img->height,
        8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png_ptr, info_ptr); // This writes all chunks before PLTE/IDAT

    // Prepare row pointers
    png_bytep *row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * img->height);
    if (!row_pointers) {
        perror("Error allocating row_pointers for write");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 0;
    }
    for (int y = 0; y < img->height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr, info_ptr));
        if (!row_pointers[y]) {
            perror("Error allocating row_pointer row for write");
            for(int k=0; k<y; ++k) free(row_pointers[k]);
            free(row_pointers);
            png_destroy_write_struct(&png_ptr, &info_ptr);
            fclose(fp);
            return 0;
        }
        // Copy data from our Image struct to libpng format
        for (int x = 0; x < img->width; x++) {
            Pixel p = img->pixels[y * img->width + x];
            png_bytep px = &(row_pointers[y][x * 4]);
            px[0] = p.r;
            px[1] = p.g;
            px[2] = p.b;
            px[3] = p.a;
        }
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during libpng png_write_image.\n");
        for (int y = 0; y < img->height; y++) free(row_pointers[y]);
        free(row_pointers);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 0;
    }

    png_write_image(png_ptr, row_pointers);

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during libpng png_write_end.\n");
         for (int y = 0; y < img->height; y++) free(row_pointers[y]);
        free(row_pointers);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return 0;
    }
    png_write_end(png_ptr, NULL); // NULL for no end_info

    // Cleanup
    for (int y = 0; y < img->height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return 1;
}


void print_png_info(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening PNG file for info");
        return;
    }

    unsigned char header[8];
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "Error: %s is not a PNG file.\n", filename);
        fclose(fp);
        return;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, user_error_fn, user_warning_fn);
    if (!png_ptr) {
        fclose(fp);
        return;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int interlace_type = png_get_interlace_type(png_ptr, info_ptr);
    // int compression_type = png_get_compression_type(png_ptr, info_ptr); // Less commonly needed for basic info
    // int filter_type = png_get_filter_type(png_ptr, info_ptr);             // "

    printf("PNG File: %s\n", filename);
    printf("  Dimensions: %d x %d pixels\n", width, height);
    printf("  Bit Depth: %d\n", bit_depth);
    printf("  Color Type: ");
    switch (color_type) {
        case PNG_COLOR_TYPE_GRAY: printf("Grayscale\n"); break;
        case PNG_COLOR_TYPE_GRAY_ALPHA: printf("Grayscale with Alpha\n"); break;
        case PNG_COLOR_TYPE_PALETTE: printf("Indexed Palette\n"); break;
        case PNG_COLOR_TYPE_RGB: printf("RGB\n"); break;
        case PNG_COLOR_TYPE_RGB_ALPHA: printf("RGBA\n"); break;
        default: printf("Unknown (%d)\n", color_type); break;
    }
    printf("  Interlace Method: %s\n", (interlace_type == PNG_INTERLACE_NONE) ? "None" : (interlace_type == PNG_INTERLACE_ADAM7) ? "Adam7" : "Unknown");
    
    // You can extract more info here if needed (e.g., gamma, sRGB, text chunks)
    // For example, to get number of tEXt chunks:
    // png_textp text_ptr;
    // int num_text;
    // if (png_get_text(png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
    //     printf("  Number of tEXt/zTXt/iTXt chunks: %d\n", num_text);
    // }


    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);
}