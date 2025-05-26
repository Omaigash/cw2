#define _GNU_SOURCE // For getopt_long if not implicitly available
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "image.h"
#include "png_handler.h"
#include "operations.h"
#include "utils.h"

// Enum for operations to ensure only one is selected
typedef enum {
    OP_NONE,
    OP_INFO,
    OP_TRIANGLE,
    OP_BIGGEST_RECT,
    OP_COLLAGE
} OperationType;

int main(int argc, char *argv[]) {
    char *input_filename = NULL;
    char *output_filename = "out.png";
    OperationType operation = OP_NONE;

    // Triangle options
    char *triangle_points_str = NULL;
    int triangle_thickness = 1;
    char *triangle_color_str = NULL;
    int triangle_fill = 0; // 0 for false, 1 for true
    char *triangle_fill_color_str = NULL;

    // Biggest_rect options
    char *rect_old_color_str = NULL;
    char *rect_new_color_str = NULL;

    // Collage options
    int collage_num_x = 0;
    int collage_num_y = 0;

    // getopt_long options
    // Note: "::" means optional argument, ":" means required argument
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"output", required_argument, 0, 'o'},
        {"info", no_argument, 0, 'i'}, // Using 'i' as a short unique char for info
        
        {"triangle", no_argument, 0, 1000}, // Custom value > 255
        {"points", required_argument, 0, 1001},
        {"thickness", required_argument, 0, 1002},
        {"color", required_argument, 0, 1003}, // General color, context dependent
        {"fill", no_argument, 0, 1004},
        {"fill_color", required_argument, 0, 1005},

        {"biggest_rect", no_argument, 0, 2000},
        {"old_color", required_argument, 0, 2001},
        {"new_color", required_argument, 0, 2002},

        {"collage", no_argument, 0, 3000},
        {"number_x", required_argument, 0, 3001},
        {"number_y", required_argument, 0, 3002},
        {0, 0, 0, 0}
    };

    int opt_char;
    int option_index = 0;
    OperationType current_op_context = OP_NONE; // To assign colors correctly

    while ((opt_char = getopt_long(argc, argv, "ho:i", long_options, &option_index)) != -1) {
        switch (opt_char) {
            case 'h':
                print_help(argv[0]);
                return EXIT_SUCCESS;
            case 'o':
                output_filename = optarg;
                break;
            case 'i': // --info
                if (operation != OP_NONE && operation != OP_INFO) {
                    fprintf(stderr, "Error: Only one main operation allowed.\n");
                    return EXIT_FAILURE;
                }
                operation = OP_INFO;
                current_op_context = OP_INFO;
                break;
            case 1000: // --triangle
                if (operation != OP_NONE && operation != OP_TRIANGLE) {
                     fprintf(stderr, "Error: Only one main operation allowed.\n"); return EXIT_FAILURE;
                }
                operation = OP_TRIANGLE;
                current_op_context = OP_TRIANGLE;
                break;
            case 1001: // --points
                triangle_points_str = optarg;
                break;
            case 1002: // --thickness
                triangle_thickness = atoi(optarg);
                if (triangle_thickness <= 0) {
                    fprintf(stderr, "Error: --thickness must be > 0.\n"); return EXIT_FAILURE;
                }
                break;
            case 1003: // --color (context dependent)
                if (current_op_context == OP_TRIANGLE) triangle_color_str = optarg;
                else {fprintf(stderr, "Error: --color used without a primary operation context (e.g. --triangle).\n"); return EXIT_FAILURE;}
                break;
            case 1004: // --fill
                triangle_fill = 1;
                break;
            case 1005: // --fill_color
                triangle_fill_color_str = optarg;
                break;
            case 2000: // --biggest_rect
                 if (operation != OP_NONE && operation != OP_BIGGEST_RECT) {
                     fprintf(stderr, "Error: Only one main operation allowed.\n"); return EXIT_FAILURE;
                }
                operation = OP_BIGGEST_RECT;
                current_op_context = OP_BIGGEST_RECT;
                break;
            case 2001: // --old_color
                rect_old_color_str = optarg;
                break;
            case 2002: // --new_color
                rect_new_color_str = optarg;
                break;
            case 3000: // --collage
                if (operation != OP_NONE && operation != OP_COLLAGE) {
                     fprintf(stderr, "Error: Only one main operation allowed.\n"); return EXIT_FAILURE;
                }
                operation = OP_COLLAGE;
                current_op_context = OP_COLLAGE;
                break;
            case 3001: // --number_x
                collage_num_x = atoi(optarg);
                 if (collage_num_x <= 0) {
                    fprintf(stderr, "Error: --number_x must be > 0.\n"); return EXIT_FAILURE;
                }
                break;
            case 3002: // --number_y
                collage_num_y = atoi(optarg);
                if (collage_num_y <= 0) {
                    fprintf(stderr, "Error: --number_y must be > 0.\n"); return EXIT_FAILURE;
                }
                break;
            case '?': // Unknown option or missing argument for short option
                // getopt_long already printed an error message
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "Warning: Unhandled option character code %d\n", opt_char);
        }
    }

    // Input file is the last non-option argument
    if (optind < argc) {
        input_filename = argv[optind];
    }

    // If no operation and no -h, print help.
    if (operation == OP_NONE) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    }
    
    // --info does not require other operations, and should not modify/output image
    if (operation == OP_INFO) {
        if (!input_filename) {
            fprintf(stderr, "Error: --info requires an input PNG file.\n");
            print_help(argv[0]);
            return EXIT_FAILURE;
        }
        print_png_info(input_filename);
        return EXIT_SUCCESS;
    }

    // All other operations require an input file
    if (!input_filename) {
        fprintf(stderr, "Error: An input PNG file is required for this operation.\n");
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    Image *img = read_png_file(input_filename);
    if (!img) {
        // read_png_file already printed an error
        return EXIT_FAILURE;
    }
    Image *output_img = img; // By default, operations modify the input image in place
                             // Except collage, which creates a new one

    // Perform the selected operation
    if (operation == OP_TRIANGLE) {
        if (!triangle_points_str || !triangle_color_str) {
            fprintf(stderr, "Error: --triangle requires --points and --color.\n"); freeImage(img); return EXIT_FAILURE;
        }
        if (triangle_fill && !triangle_fill_color_str) {
            fprintf(stderr, "Error: --fill for triangle also requires --fill_color.\n"); freeImage(img); return EXIT_FAILURE;
        }
        
        int x1,y1,x2,y2,x3,y3;
        RGBColor line_c, fill_c = {0,0,0}; // Initialize fill_c
        
        if (!parse_triangle_points(triangle_points_str, &x1, &y1, &x2, &y2, &x3, &y3)) {
            freeImage(img); return EXIT_FAILURE;
        }
        if (!parse_color_string(triangle_color_str, &line_c)) {
            freeImage(img); return EXIT_FAILURE;
        }
        if (triangle_fill) {
            if (!parse_color_string(triangle_fill_color_str, &fill_c)) {
                freeImage(img); return EXIT_FAILURE;
            }
        }
        op_draw_triangle(img, x1, y1, x2, y2, x3, y3, triangle_thickness, line_c, triangle_fill, fill_c);
    } 
    else if (operation == OP_BIGGEST_RECT) {
        if (!rect_old_color_str || !rect_new_color_str) {
            fprintf(stderr, "Error: --biggest_rect requires --old_color and --new_color.\n"); freeImage(img); return EXIT_FAILURE;
        }
        RGBColor old_c, new_c;
        if (!parse_color_string(rect_old_color_str, &old_c) || !parse_color_string(rect_new_color_str, &new_c)) {
            freeImage(img); return EXIT_FAILURE;
        }
        op_biggest_rect(img, old_c, new_c);
    } 
    else if (operation == OP_COLLAGE) {
        if (collage_num_x == 0 || collage_num_y == 0) {
            fprintf(stderr, "Error: --collage requires --number_x and --number_y to be > 0.\n"); freeImage(img); return EXIT_FAILURE;
        }
        Image* collage_result = op_collage(img, collage_num_x, collage_num_y);
        if (!collage_result) {
            fprintf(stderr, "Error: Failed to create collage.\n"); freeImage(img); return EXIT_FAILURE;
        }
        freeImage(img); // Free the original loaded image
        output_img = collage_result; // The new collage is now the one to save
    }

    // Save the resulting image
    if (!write_png_file(output_filename, output_img)) {
        fprintf(stderr, "Error: Failed to write output PNG file '%s'.\n", output_filename);
        freeImage(output_img); // output_img might be img or collage_result
        return EXIT_FAILURE;
    }

    printf("Operation successful. Output saved to %s\n", output_filename);
    freeImage(output_img);

    return EXIT_SUCCESS;
}