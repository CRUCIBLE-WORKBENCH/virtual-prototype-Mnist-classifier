
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
#
# ======================================================================
# Ignytion Accelerator IP -- Renode Python Peripheral Model
# ======================================================================
#
# A custom AI/ML accelerator peripheral for the LiteX+VexRiscv VP demo.
# Supports dot product, elementwise add, and ReLU operations on vectors.
#
# Base address: 0xF0010000 (configured in .repl)
# Register size: 0x100 (256 bytes)
#
# Register Map:
#   0x00  CONTROL      R/W   [0] START  [1] RESET  [2] IRQ_EN
#   0x04  STATUS       R     [0] BUSY   [1] DONE   [2] ERR   [31:16] loaded count
#   0x08  CONFIG       R/W   [15:0] VECTOR_LEN  [23:16] OP_MODE
#   0x0C  INPUT_A      W     Write port for vector A (FIFO)
#   0x10  INPUT_B      W     Write port for vector B (FIFO)
#   0x14  OUTPUT       R     Read port for results (FIFO)
#   0x18  RESULT_COUNT R     Number of results available
#   0x1C  CYCLE_COUNT  R     Simulated compute latency
#   0x20  VERSION      R     0x49474E01 ("IGN" + v01)
#   0x24  SIGNATURE    R     0xDEADBEEF
#
# Operation Modes:
#   0x00  Dot Product     -- scalar result: sum(A[i] * B[i])
#   0x01  Elementwise Add -- vector result: A[i] + B[i]
#   0x02  ReLU            -- vector result: max(0, A[i])
#
# =============================================================================

# [NOTE] !!!! Renode's Python peripherals use **IronPython** (Python 2 syntax).
# `print` isn't used (we use `self.NoisyLog`), no f-strings, no Python 3-only features.

# --- Register offsets ---
REG_CONTROL      = 0x00
REG_STATUS       = 0x04
REG_CONFIG       = 0x08
REG_INPUT_A      = 0x0C
REG_INPUT_B      = 0x10
REG_OUTPUT       = 0x14
REG_RESULT_COUNT = 0x18
REG_CYCLE_COUNT  = 0x1C
REG_VERSION      = 0x20
REG_SIGNATURE    = 0x24

# --- Operation modes ---
OP_DOT_PRODUCT    = 0x00
OP_ELEMENTWISE_ADD = 0x01
OP_RELU           = 0x02

# --- Status bits ---
STATUS_BUSY = (1 << 0)
STATUS_DONE = (1 << 1)
STATUS_ERR  = (1 << 2)

# --- Constants ---
IP_VERSION   = 0x49474E01   # "IGN" + version 01
IP_SIGNATURE = 0xDEADBEEF
MAX_VECTOR_LEN = 1024

# =============================================================================
# Initialization
# =============================================================================
if request.IsInit:
    vector_len = 0
    op_mode = OP_DOT_PRODUCT
    irq_en = 0

    fifo_a = []
    fifo_b = []
    fifo_out = []

    busy = 0
    done = 0
    err = 0
    elements_loaded = 0
    cycle_count = 0

    self.NoisyLog("IgnytionAccel: Initialized (VERSION=0x%08X)" % IP_VERSION)

# =============================================================================
# Write handling
# =============================================================================
elif request.IsWrite:
    offset = request.Offset
    val = request.Value

    # --- CONTROL register ---
    if offset == REG_CONTROL:
        # Bit 1: RESET -- clear all state
        if val & 0x2:
            fifo_a = []
            fifo_b = []
            fifo_out = []
            busy = 0
            done = 0
            err = 0
            elements_loaded = 0
            cycle_count = 0
            self.NoisyLog("IgnytionAccel: RESET")

        # Bit 2: IRQ_EN
        irq_en = (val >> 2) & 0x1

        # Bit 0: START -- trigger computation
        if val & 0x1:
            if busy:
                self.NoisyLog("IgnytionAccel: START while BUSY -- ignored")
            elif vector_len == 0:
                err = 1
                self.NoisyLog("IgnytionAccel: START with vector_len=0 -- ERROR")
            else:
                busy = 1
                done = 0
                err = 0
                fifo_out = []

                self.NoisyLog("IgnytionAccel: START op_mode=%d vector_len=%d" % (op_mode, vector_len))
                self.NoisyLog("IgnytionAccel:   A = %s" % str(fifo_a[:vector_len]))
                self.NoisyLog("IgnytionAccel:   B = %s" % str(fifo_b[:vector_len]))

                # --- Dot Product ---
                if op_mode == OP_DOT_PRODUCT:
                    if len(fifo_a) < vector_len or len(fifo_b) < vector_len:
                        err = 1
                        busy = 0
                        self.NoisyLog("IgnytionAccel: ERROR -- insufficient data (A:%d B:%d, need %d)" % (len(fifo_a), len(fifo_b), vector_len))
                    else:
                        result = 0
                        for i in range(vector_len):
                            # Handle signed 32-bit multiplication
                            a_val = fifo_a[i] if fifo_a[i] < 0x80000000 else fifo_a[i] - 0x100000000
                            b_val = fifo_b[i] if fifo_b[i] < 0x80000000 else fifo_b[i] - 0x100000000
                            result += a_val * b_val
                        # Truncate to 32-bit (handle negative results)
                        result = result & 0xFFFFFFFF
                        fifo_out = [result]
                        cycle_count = vector_len * 2
                        busy = 0
                        done = 1
                        self.NoisyLog("IgnytionAccel: DOT PRODUCT result = %d (0x%08X) in %d cycles" % (result if result < 0x80000000 else result - 0x100000000, result, cycle_count))

                # --- Elementwise Add ---
                elif op_mode == OP_ELEMENTWISE_ADD:
                    if len(fifo_a) < vector_len or len(fifo_b) < vector_len:
                        err = 1
                        busy = 0
                        self.NoisyLog("IgnytionAccel: ERROR -- insufficient data for EW_ADD")
                    else:
                        fifo_out = []
                        for i in range(vector_len):
                            a_val = fifo_a[i] if fifo_a[i] < 0x80000000 else fifo_a[i] - 0x100000000
                            b_val = fifo_b[i] if fifo_b[i] < 0x80000000 else fifo_b[i] - 0x100000000
                            fifo_out.append((a_val + b_val) & 0xFFFFFFFF)
                        cycle_count = vector_len * 1
                        busy = 0
                        done = 1
                        self.NoisyLog("IgnytionAccel: EW_ADD result = %s in %d cycles" % (str(fifo_out), cycle_count))

                # --- ReLU ---
                elif op_mode == OP_RELU:
                    if len(fifo_a) < vector_len:
                        err = 1
                        busy = 0
                        self.NoisyLog("IgnytionAccel: ERROR -- insufficient data for RELU")
                    else:
                        fifo_out = []
                        for i in range(vector_len):
                            a_val = fifo_a[i] if fifo_a[i] < 0x80000000 else fifo_a[i] - 0x100000000
                            fifo_out.append(a_val if a_val > 0 else 0)
                        cycle_count = vector_len * 1
                        busy = 0
                        done = 1
                        self.NoisyLog("IgnytionAccel: RELU result = %s in %d cycles" % (str(fifo_out), cycle_count))

                # --- Unknown mode ---
                else:
                    err = 1
                    busy = 0
                    self.NoisyLog("IgnytionAccel: ERROR -- unknown op_mode %d" % op_mode)

    # --- CONFIG register ---
    elif offset == REG_CONFIG:
        vector_len = val & 0xFFFF
        op_mode = (val >> 16) & 0xFF
        if vector_len > MAX_VECTOR_LEN:
            vector_len = MAX_VECTOR_LEN
        # Reset FIFOs on reconfiguration
        fifo_a = []
        fifo_b = []
        fifo_out = []
        elements_loaded = 0
        done = 0
        err = 0
        self.NoisyLog("IgnytionAccel: CONFIG vector_len=%d op_mode=%d" % (vector_len, op_mode))

    # --- INPUT_A write port (FIFO) ---
    elif offset == REG_INPUT_A:
        if len(fifo_a) < vector_len:
            fifo_a.append(val & 0xFFFFFFFF)
            elements_loaded = len(fifo_a) + len(fifo_b)
        else:
            self.NoisyLog("IgnytionAccel: WARNING -- INPUT_A overflow (already %d elements)" % len(fifo_a))

    # --- INPUT_B write port (FIFO) ---
    elif offset == REG_INPUT_B:
        if len(fifo_b) < vector_len:
            fifo_b.append(val & 0xFFFFFFFF)
            elements_loaded = len(fifo_a) + len(fifo_b)
        else:
            self.NoisyLog("IgnytionAccel: WARNING -- INPUT_B overflow (already %d elements)" % len(fifo_b))

    # --- All other writes ignored ---
    else:
        self.NoisyLog("IgnytionAccel: WRITE to read-only/unknown offset 0x%02X ignored" % offset)

# =============================================================================
# Read handling
# =============================================================================
elif request.IsRead:
    offset = request.Offset

    # --- CONTROL register (read back current config) ---
    if offset == REG_CONTROL:
        request.Value = (irq_en << 2)

    # --- STATUS register ---
    elif offset == REG_STATUS:
        status = 0
        if busy:
            status |= STATUS_BUSY
        if done:
            status |= STATUS_DONE
        if err:
            status |= STATUS_ERR
        status |= (elements_loaded & 0xFFFF) << 16
        request.Value = status

    # --- CONFIG register ---
    elif offset == REG_CONFIG:
        request.Value = (op_mode << 16) | (vector_len & 0xFFFF)

    # --- OUTPUT read port (FIFO) ---
    elif offset == REG_OUTPUT:
        if len(fifo_out) > 0:
            request.Value = fifo_out.pop(0) & 0xFFFFFFFF
        else:
            request.Value = 0
            self.NoisyLog("IgnytionAccel: WARNING -- OUTPUT read on empty FIFO")

    # --- RESULT_COUNT ---
    elif offset == REG_RESULT_COUNT:
        request.Value = len(fifo_out)

    # --- CYCLE_COUNT ---
    elif offset == REG_CYCLE_COUNT:
        request.Value = cycle_count

    # --- VERSION ---
    elif offset == REG_VERSION:
        request.Value = IP_VERSION

    # --- SIGNATURE ---
    elif offset == REG_SIGNATURE:
        request.Value = IP_SIGNATURE

    # --- Unknown register ---
    else:
        request.Value = 0
        self.NoisyLog("IgnytionAccel: READ from unknown offset 0x%02X" % offset)
