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
 *
 *  Ignytion Accelerator IP -- Test & Demo Program
 *  Runs inside LiteX+VexRiscv Linux VP (Renode)
 *
 *  Communicates with the accelerator at 0xF0010000 via /dev/mem MMIO.
 *  Demonstrates dot product, elementwise add, and ReLU operations.
 *
 * ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

/* ---- Accelerator base address and register map ---- */
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

/* ---- Control bits ---- */
#define CTRL_START       (1 << 0)
#define CTRL_RESET       (1 << 1)
#define CTRL_IRQ_EN      (1 << 2)

/* ---- Status bits ---- */
#define STATUS_BUSY      (1 << 0)
#define STATUS_DONE      (1 << 1)
#define STATUS_ERR       (1 << 2)

/* ---- Operation modes ---- */
#define OP_DOT_PRODUCT      0x00
#define OP_ELEMENTWISE_ADD  0x01
#define OP_RELU             0x02

/* ---- ANSI colors ---- */
#define SPARK   "\033[38;5;172m"
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RESET   "\033[0m"

/* ---- Global mmap pointer ---- */
static volatile uint32_t *accel = NULL;

/* ---- Register access helpers ---- */
static inline void reg_write(uint32_t offset, uint32_t value)
{
    accel[offset / 4] = value;
}

static inline uint32_t reg_read(uint32_t offset)
{
    return accel[offset / 4];
}

/* ---- Accelerator operations ---- */
static void accel_reset(void)
{
    reg_write(REG_CONTROL, CTRL_RESET);
}

static void accel_configure(uint16_t vector_len, uint8_t op_mode)
{
    uint32_t config = ((uint32_t)op_mode << 16) | (vector_len & 0xFFFF);
    reg_write(REG_CONFIG, config);
}

static void accel_load_a(const int32_t *data, int len)
{
    int i;
    for (i = 0; i < len; i++)
        reg_write(REG_INPUT_A, (uint32_t)data[i]);
}

static void accel_load_b(const int32_t *data, int len)
{
    int i;
    for (i = 0; i < len; i++)
        reg_write(REG_INPUT_B, (uint32_t)data[i]);
}

static void accel_start(void)
{
    reg_write(REG_CONTROL, CTRL_START);
}

static int accel_wait_done(void)
{
    uint32_t status = reg_read(REG_STATUS);
    if (status & STATUS_ERR)
        return -1;
    if (status & STATUS_DONE)
        return 0;
    /* Should not reach here in our synchronous model */
    return -2;
}

static uint32_t accel_read_result(void)
{
    return reg_read(REG_OUTPUT);
}

static uint32_t accel_result_count(void)
{
    return reg_read(REG_RESULT_COUNT);
}

static uint32_t accel_cycle_count(void)
{
    return reg_read(REG_CYCLE_COUNT);
}

/* ---- Display helpers ---- */
static void print_header(void)
{
    printf("\n");
    printf(SPARK "==========================================================\n");
    printf("   Ignytion Accelerator IP -- Functional Validation\n");
    printf("==========================================================" RESET "\n");
    printf(DIM "   Base: 0x%08X  |  Signature: 0x%08X  |  Version: 0x%08X\n" RESET,
           ACCEL_BASE, reg_read(REG_SIGNATURE), reg_read(REG_VERSION));
    printf("\n");
}

static void print_vector_i32(const char *name, const int32_t *v, int len)
{
    int i;
    printf("   %s = [", name);
    for (i = 0; i < len; i++) {
        if (i > 0) printf(", ");
        printf("%d", v[i]);
    }
    printf("]\n");
}

static void print_test_header(int num, const char *name)
{
    printf(SPARK "-- Test %d: %s " RESET, num, name);
    printf(DIM "------------------------------------------" RESET "\n");
}

static void print_pass(const char *detail)
{
    printf(GREEN BOLD "   PASS" RESET " %s\n", detail);
}

static void print_fail(const char *detail)
{
    printf(RED BOLD "   FAIL" RESET " %s\n", detail);
}

/* ---- Test: Dot Product ---- */
static int test_dot_product(void)
{
    int32_t vec_a[] = {1, 2, 3, 4};
    int32_t vec_b[] = {5, 6, 7, 8};
    int len = 4;
    int32_t expected = 1*5 + 2*6 + 3*7 + 4*8;  /* = 70 */

    print_test_header(1, "Dot Product");
    print_vector_i32("A", vec_a, len);
    print_vector_i32("B", vec_b, len);
    printf("   Expected: %d\n", expected);
    printf("\n");

    accel_reset();
    accel_configure(len, OP_DOT_PRODUCT);
    accel_load_a(vec_a, len);
    accel_load_b(vec_b, len);
    accel_start();

    if (accel_wait_done() != 0) {
        print_fail("Accelerator reported error");
        return 1;
    }

    int32_t result = (int32_t)accel_read_result();
    uint32_t cycles = accel_cycle_count();

    printf("   Result:   %d", result);
    if (result == expected)
        printf(GREEN " (correct)" RESET);
    else
        printf(RED " (WRONG -- expected %d)" RESET, expected);
    printf("\n");
    printf(DIM "   Cycles:   %u\n" RESET, cycles);
    printf("\n");

    if (result == expected) {
        print_pass("Dot product verified");
        return 0;
    } else {
        print_fail("Dot product mismatch");
        return 1;
    }
}

/* ---- Test: Dot Product with negative values ---- */
static int test_dot_product_signed(void)
{
    int32_t vec_a[] = {-1, 2, -3, 4};
    int32_t vec_b[] = {5, -6, 7, -8};
    int len = 4;
    int32_t expected = (-1)*5 + 2*(-6) + (-3)*7 + 4*(-8);  /* = -5-12-21-32 = -70 */

    print_test_header(2, "Dot Product (signed)");
    print_vector_i32("A", vec_a, len);
    print_vector_i32("B", vec_b, len);
    printf("   Expected: %d\n", expected);
    printf("\n");

    accel_reset();
    accel_configure(len, OP_DOT_PRODUCT);
    accel_load_a(vec_a, len);
    accel_load_b(vec_b, len);
    accel_start();

    if (accel_wait_done() != 0) {
        print_fail("Accelerator reported error");
        return 1;
    }

    int32_t result = (int32_t)accel_read_result();
    uint32_t cycles = accel_cycle_count();

    printf("   Result:   %d", result);
    if (result == expected)
        printf(GREEN " (correct)" RESET);
    else
        printf(RED " (WRONG -- expected %d)" RESET, expected);
    printf("\n");
    printf(DIM "   Cycles:   %u\n" RESET, cycles);
    printf("\n");

    if (result == expected) {
        print_pass("Signed dot product verified");
        return 0;
    } else {
        print_fail("Signed dot product mismatch");
        return 1;
    }
}

/* ---- Test: Elementwise Add ---- */
static int test_elementwise_add(void)
{
    int32_t vec_a[] = {10, 20, 30};
    int32_t vec_b[] = {1, 2, 3};
    int32_t expected[] = {11, 22, 33};
    int len = 3;
    int i;
    int pass = 1;

    print_test_header(3, "Elementwise Add");
    print_vector_i32("A", vec_a, len);
    print_vector_i32("B", vec_b, len);
    print_vector_i32("Expected", expected, len);
    printf("\n");

    accel_reset();
    accel_configure(len, OP_ELEMENTWISE_ADD);
    accel_load_a(vec_a, len);
    accel_load_b(vec_b, len);
    accel_start();

    if (accel_wait_done() != 0) {
        print_fail("Accelerator reported error");
        return 1;
    }

    uint32_t count = accel_result_count();
    uint32_t cycles = accel_cycle_count();

    printf("   Results (%u elements):", count);
    printf("  [");
    for (i = 0; i < len; i++) {
        int32_t val = (int32_t)accel_read_result();
        if (i > 0) printf(", ");
        printf("%d", val);
        if (val != expected[i])
            pass = 0;
    }
    printf("]");
    if (pass)
        printf(GREEN " (correct)" RESET);
    else
        printf(RED " (WRONG)" RESET);
    printf("\n");
    printf(DIM "   Cycles:   %u\n" RESET, cycles);
    printf("\n");

    if (pass) {
        print_pass("Elementwise add verified");
        return 0;
    } else {
        print_fail("Elementwise add mismatch");
        return 1;
    }
}

/* ---- Test: ReLU ---- */
static int test_relu(void)
{
    int32_t vec_a[] = {5, -3, 0, 7, -10};
    int32_t expected[] = {5, 0, 0, 7, 0};
    int len = 5;
    int i;
    int pass = 1;

    print_test_header(4, "ReLU (Activation Function)");
    print_vector_i32("A", vec_a, len);
    print_vector_i32("Expected", expected, len);
    printf("\n");

    accel_reset();
    accel_configure(len, OP_RELU);
    accel_load_a(vec_a, len);
    accel_start();

    if (accel_wait_done() != 0) {
        print_fail("Accelerator reported error");
        return 1;
    }

    uint32_t count = accel_result_count();
    uint32_t cycles = accel_cycle_count();

    printf("   Results (%u elements):", count);
    printf("  [");
    for (i = 0; i < len; i++) {
        int32_t val = (int32_t)accel_read_result();
        if (i > 0) printf(", ");
        printf("%d", val);
        if (val != expected[i])
            pass = 0;
    }
    printf("]");
    if (pass)
        printf(GREEN " (correct)" RESET);
    else
        printf(RED " (WRONG)" RESET);
    printf("\n");
    printf(DIM "   Cycles:   %u\n" RESET, cycles);
    printf("\n");

    if (pass) {
        print_pass("ReLU verified");
        return 0;
    } else {
        print_fail("ReLU mismatch");
        return 1;
    }
}

/* ---- Main ---- */
int main(int argc, char *argv[])
{
    int fd;
    int failures = 0;

    /* Open /dev/mem for MMIO access */
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        printf("Must run as root.\n");
        return 1;
    }

    /* mmap the accelerator register space */
    accel = (volatile uint32_t *)mmap(
        NULL, ACCEL_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd, ACCEL_BASE
    );

    if (accel == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    /* Verify the accelerator is present */
    uint32_t sig = reg_read(REG_SIGNATURE);
    if (sig != 0xDEADBEEF) {
        printf(RED "ERROR: Accelerator not found at 0x%08X (signature=0x%08X)\n" RESET,
               ACCEL_BASE, sig);
        munmap((void *)accel, ACCEL_SIZE);
        close(fd);
        return 1;
    }

    print_header();

    /* Run all tests */
    failures += test_dot_product();
    failures += test_dot_product_signed();
    failures += test_elementwise_add();
    failures += test_relu();

    /* Summary */
    printf(SPARK "==========================================================\n");
    if (failures == 0) {
        printf(GREEN BOLD "   ALL TESTS PASSED" RESET SPARK " -- Accelerator IP functional\n");
    } else {
        printf(RED BOLD "   %d TEST(S) FAILED" RESET SPARK "\n", failures);
    }
    printf("==========================================================" RESET "\n\n");

    munmap((void *)accel, ACCEL_SIZE);
    close(fd);
    return failures;
}
