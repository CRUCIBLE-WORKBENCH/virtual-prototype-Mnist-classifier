/* -------------------------------------------------------------------------
 *
 *  IGNYTION IO
 *  Copyright (c) 2026 IGNYTION IO. All Rights Reserved.
 *
 *  This software and associated documentation files are proprietary
 *  and confidential. Unauthorized copying, modification, distribution,
 *  or use of this file, via any medium, is strictly prohibited.
 *
 *  Website: https://ignytion.io
 *
 *  MNIST Classifier -- Accelerator-Driven Inference
 *  Runs inside LiteX+VexRiscv Linux VP (Renode)
 *
 *  Uses the Ignytion Accelerator IP at 0xF0010000 for:
 *    - Dot products (Layer 1 and Layer 2 forward pass)
 *    - ReLU activation (Layer 1 output)
 *
 *  Model: FC 784 -> 64 (ReLU) -> 10
 *  Weights: int8 quantized, exported by train_mnist.py
 *
 * ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "mnist_model.h"

/* ---- Accelerator registers ---- */
#define ACCEL_BASE       0xF0010000
#define ACCEL_SIZE       0x100

#define REG_CONTROL      0x00
#define REG_STATUS       0x04
#define REG_CONFIG       0x08
#define REG_INPUT_A      0x0C
#define REG_INPUT_B      0x10
#define REG_OUTPUT       0x14
#define REG_RESULT_COUNT 0x18
#define REG_CYCLE_COUNT  0x1C
#define REG_VERSION      0x20
#define REG_SIGNATURE    0x24

#define CTRL_START       (1 << 0)
#define CTRL_RESET       (1 << 1)
#define STATUS_DONE      (1 << 1)
#define STATUS_ERR       (1 << 2)

#define OP_DOT_PRODUCT   0x00
#define OP_RELU          0x02

/* ---- ANSI colors ---- */
#define SPARK   "\033[38;5;172m"
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RESET   "\033[0m"

/* ---- Accelerator access ---- */
static volatile uint32_t *accel = NULL;
static uint32_t total_accel_calls = 0;
static uint32_t total_accel_cycles = 0;

static inline void reg_write(uint32_t offset, uint32_t value)
{
    accel[offset / 4] = value;
}

static inline uint32_t reg_read(uint32_t offset)
{
    return accel[offset / 4];
}

static void accel_reset(void)
{
    reg_write(REG_CONTROL, CTRL_RESET);
}

static void accel_configure(int vector_len, int op_mode)
{
    uint32_t config = ((uint32_t)op_mode << 16) | (vector_len & 0xFFFF);
    reg_write(REG_CONFIG, config);
}

/* ---- Dot product: int8 weights x uint8 pixels ---- */
static int32_t accel_dot_i8_u8(const int8_t *weights, const uint8_t *input, int len)
{
    int i;

    accel_reset();
    accel_configure(len, OP_DOT_PRODUCT);

    /* Load weights (int8 -> sign-extended int32) */
    for (i = 0; i < len; i++)
        reg_write(REG_INPUT_A, (uint32_t)(int32_t)weights[i]);

    /* Load input (uint8 -> zero-extended int32) */
    for (i = 0; i < len; i++)
        reg_write(REG_INPUT_B, (uint32_t)input[i]);

    /* Start computation */
    reg_write(REG_CONTROL, CTRL_START);

    total_accel_calls++;
    total_accel_cycles += reg_read(REG_CYCLE_COUNT);

    return (int32_t)reg_read(REG_OUTPUT);
}

/* ---- Dot product: int8 weights x int32 hidden ---- */
static int32_t accel_dot_i8_i32(const int8_t *weights, const int32_t *input, int len)
{
    int i;

    accel_reset();
    accel_configure(len, OP_DOT_PRODUCT);

    /* Load weights (int8 -> sign-extended int32) */
    for (i = 0; i < len; i++)
        reg_write(REG_INPUT_A, (uint32_t)(int32_t)weights[i]);

    /* Load hidden values (int32, positive after ReLU) */
    for (i = 0; i < len; i++)
        reg_write(REG_INPUT_B, (uint32_t)input[i]);

    reg_write(REG_CONTROL, CTRL_START);

    total_accel_calls++;
    total_accel_cycles += reg_read(REG_CYCLE_COUNT);

    return (int32_t)reg_read(REG_OUTPUT);
}

/* ---- Print digit as ASCII art ---- */
static void print_digit(const uint8_t *pixels)
{
    int row, col;
    for (row = 0; row < 28; row++) {
        printf("   ");
        for (col = 0; col < 28; col++) {
            uint8_t p = pixels[row * 28 + col];
            if (p > 200)      printf("##");
            else if (p > 150) printf("%%");
            else if (p > 100) printf("++");
            else if (p > 50)  printf("..");
            else              printf("  ");
        }
        printf("\n");
    }
}

/* ---- Classify one image ---- */
static int classify_image(const uint8_t *pixels)
{
    int32_t hidden[L1_OUT_SIZE];
    int32_t output[L2_OUT_SIZE];
    int j;

    /* ---- Layer 1: FC 784 -> 64 ---- */
    for (j = 0; j < L1_OUT_SIZE; j++) {
        /* Dot product via accelerator: w1[j] (int8) . pixels (uint8) */
        int32_t acc = accel_dot_i8_u8(l1_weights[j], pixels, L1_IN_SIZE);

        /* Add bias */
        acc += l1_bias[j];

        /* Rescale to prevent Layer 2 overflow */
        acc = acc >> L1_SHIFT;

        /* ReLU */
        hidden[j] = (acc > 0) ? acc : 0;
    }

    /* ---- Layer 2: FC 64 -> 10 ---- */
    for (j = 0; j < L2_OUT_SIZE; j++) {
        /* Dot product via accelerator: w2[j] (int8) . hidden (int32) */
        output[j] = accel_dot_i8_i32(l2_weights[j], hidden, L2_IN_SIZE);

        /* Add bias */
        output[j] += l2_bias[j];
    }

    /* ---- Argmax ---- */
    int predicted = 0;
    for (j = 1; j < L2_OUT_SIZE; j++) {
        if (output[j] > output[predicted])
            predicted = j;
    }

    return predicted;
}

/* ---- Main ---- */
int main(int argc, char *argv[])
{
    int fd, i;
    int correct = 0;
    int show_images = 1;

    /* Parse args */
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'q')
        show_images = 0;  /* -q = quiet, no ASCII art */

    /* ---- Open accelerator via /dev/mem ---- */
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        printf("Must run as root.\n");
        return 1;
    }

    accel = (volatile uint32_t *)mmap(
        NULL, ACCEL_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, ACCEL_BASE);

    if (accel == MAP_FAILED) {
        perror("mmap accelerator");
        close(fd);
        return 1;
    }

    /* ---- Verify accelerator presence ---- */
    uint32_t sig = reg_read(REG_SIGNATURE);
    uint32_t ver = reg_read(REG_VERSION);
    if (sig != 0xDEADBEEF) {
        printf(RED "Accelerator not found at 0x%08X (sig=0x%08X)\n" RESET,
               ACCEL_BASE, sig);
        munmap((void *)accel, ACCEL_SIZE);
        close(fd);
        return 1;
    }

    /* ---- Header ---- */
    printf("\n");
    printf(SPARK "==========================================================\n");
    printf("   MNIST Handwritten Digit Classifier\n");
    printf("   Ignytion Accelerator-Driven Inference\n");
    printf("==========================================================" RESET "\n");
    printf(DIM "   Model:        FC 784 -> 64 (ReLU) -> 10\n");
    printf("   Quantization: int8 weights, int32 accumulators\n");
    printf("   Accelerator:  0x%08X (sig=0x%08X ver=0x%08X)\n" RESET,
           ACCEL_BASE, sig, ver);
    printf("\n");

    /* ---- Classify each test image ---- */
    printf(SPARK "-- Classifying %d test images " RESET
           DIM "------------------------------" RESET "\n\n", NUM_TEST_IMAGES);

    for (i = 0; i < NUM_TEST_IMAGES; i++) {
        const uint8_t *pixels = test_images[i];
        int label = test_labels[i];

        if (show_images) {
            printf(DIM "   --- Image %d (label=%d) ---" RESET "\n", i, label);
            print_digit(pixels);
        }

        int predicted = classify_image(pixels);
        int is_correct = (predicted == label);
        if (is_correct) correct++;

        if (is_correct) {
            printf("   Image %d:  Predicted=" BOLD "%d" RESET
                   "  Actual=" BOLD "%d" RESET
                   GREEN "  [CORRECT]" RESET "\n", i, predicted, label);
        } else {
            printf("   Image %d:  Predicted=" BOLD "%d" RESET
                   "  Actual=" BOLD "%d" RESET
                   RED "  [WRONG]" RESET "\n", i, predicted, label);
        }

        if (show_images)
            printf("\n");
    }

    /* ---- The VexRiscv is configured as RV32IMA — no F extension. There's no hardware floating point.  */
    int acc_pct = (correct * 100) / NUM_TEST_IMAGES;
    int acc_frac = ((correct * 1000) / NUM_TEST_IMAGES) % 10;

    printf(SPARK "\n==========================================================\n");
    printf("   Results\n");
    printf("==========================================================" RESET "\n");
    printf("   Accuracy:    %d/%d (%d.%d%%)\n", correct, NUM_TEST_IMAGES, acc_pct, acc_frac);
    printf("   Accel calls: %u (L1: %d x %d dot products, L2: %d x %d dot products)\n",
           total_accel_calls,
           NUM_TEST_IMAGES, L1_OUT_SIZE,
           NUM_TEST_IMAGES, L2_OUT_SIZE);
    printf("   Accel cycles: %u (simulated)\n", total_accel_cycles);
    printf(SPARK "==========================================================\n" RESET);
    printf("\n");

    munmap((void *)accel, ACCEL_SIZE);
    close(fd);
    return (correct == NUM_TEST_IMAGES) ? 0 : 1;
}
