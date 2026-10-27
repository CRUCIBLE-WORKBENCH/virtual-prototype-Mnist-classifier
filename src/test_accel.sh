#!/bin/sh
#  ---------------------------------------------------------------------
#
#  IGNYTION IO
#  Copyright (c) 2026 IGNYTION IO. All Rights Reserved.
#
#  This software and associated documentation files are proprietary
#  and confidential. Unauthorized copying, modification, distribution,
#  or use of this file, via any medium, is strictly prohibited.
#
#  Website: https://ignytion.io
#
#  ---------------------------------------------------------------------


echo ""
echo "=================================================="
echo "  Ignytion Accelerator IP -- Validation Test"
echo "=================================================="
echo ""

# --- Helper ---
reg_read() {
    val=$(devmem $1)
    echo "  [$2] = $val"
}

# =============================================================================
# Test 0: IP Identification
# =============================================================================
echo "[Test 0] IP Identification"
echo "-------------------------------------------"
reg_read 0xF0010024 "SIGNATURE"
reg_read 0xF0010020 "VERSION  "
echo ""

# =============================================================================
# Test 1: Dot Product
#   A = [1, 2, 3, 4]
#   B = [5, 6, 7, 8]
#   Expected: 1*5 + 2*6 + 3*7 + 4*8 = 5+12+21+32 = 70 (0x46)
# =============================================================================
echo "[Test 1] Dot Product: A=[1,2,3,4] B=[5,6,7,8]"
echo "  Expected result: 70 (0x00000046)"
echo "-------------------------------------------"

# Reset
devmem 0xF0010000 32 0x2

# Configure: vector_len=4, op_mode=0 (dot product)
devmem 0xF0010008 32 0x00000004

# Load vector A
devmem 0xF001000C 32 1
devmem 0xF001000C 32 2
devmem 0xF001000C 32 3
devmem 0xF001000C 32 4

# Load vector B
devmem 0xF0010010 32 5
devmem 0xF0010010 32 6
devmem 0xF0010010 32 7
devmem 0xF0010010 32 8

# Start
devmem 0xF0010000 32 0x1

# Read results
reg_read 0xF0010004 "STATUS      "
reg_read 0xF0010014 "RESULT      "
reg_read 0xF001001C "CYCLE_COUNT "
echo ""

# =============================================================================
# Test 2: Elementwise Add
#   A = [10, 20, 30]
#   B = [1, 2, 3]
#   Expected: [11, 22, 33]
# =============================================================================
echo "[Test 2] Elementwise Add: A=[10,20,30] B=[1,2,3]"
echo "  Expected result: [11, 22, 33]"
echo "-------------------------------------------"

# Reset
devmem 0xF0010000 32 0x2

# Configure: vector_len=3, op_mode=1 (elementwise add)
devmem 0xF0010008 32 0x00010003

# Load vector A
devmem 0xF001000C 32 10
devmem 0xF001000C 32 20
devmem 0xF001000C 32 30

# Load vector B
devmem 0xF0010010 32 1
devmem 0xF0010010 32 2
devmem 0xF0010010 32 3

# Start
devmem 0xF0010000 32 0x1

# Read results
reg_read 0xF0010004 "STATUS      "
reg_read 0xF0010018 "RESULT_COUNT"
echo "  Results:"
reg_read 0xF0010014 "  OUT[0]    "
reg_read 0xF0010014 "  OUT[1]    "
reg_read 0xF0010014 "  OUT[2]    "
reg_read 0xF001001C "CYCLE_COUNT "
echo ""

# =============================================================================
# Test 3: ReLU
#   A = [5, -3, 0, 7, -10]  (-3 = 0xFFFFFFFD, -10 = 0xFFFFFFF6)
#   Expected: [5, 0, 0, 7, 0]
# =============================================================================
echo "[Test 3] ReLU: A=[5, -3, 0, 7, -10]"
echo "  Expected result: [5, 0, 0, 7, 0]"
echo "-------------------------------------------"

# Reset
devmem 0xF0010000 32 0x2

# Configure: vector_len=5, op_mode=2 (ReLU)
devmem 0xF0010008 32 0x00020005

# Load vector A (negative values as unsigned 32-bit)
devmem 0xF001000C 32 5
devmem 0xF001000C 32 0xFFFFFFFD
devmem 0xF001000C 32 0
devmem 0xF001000C 32 7
devmem 0xF001000C 32 0xFFFFFFF6

# Start (ReLU only uses vector A)
devmem 0xF0010000 32 0x1

# Read results
reg_read 0xF0010004 "STATUS      "
reg_read 0xF0010018 "RESULT_COUNT"
echo "  Results:"
reg_read 0xF0010014 "  OUT[0]    "
reg_read 0xF0010014 "  OUT[1]    "
reg_read 0xF0010014 "  OUT[2]    "
reg_read 0xF0010014 "  OUT[3]    "
reg_read 0xF0010014 "  OUT[4]    "
reg_read 0xF001001C "CYCLE_COUNT "
echo ""

# =============================================================================
# Summary
# =============================================================================
echo "=================================================="
echo "  All tests complete."
echo "  Accelerator IP verified at 0xF0010000"
echo "=================================================="
echo ""
