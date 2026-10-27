#!/usr/bin/env python3

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

"""
IGNYTION IO -- MNIST Model Trainer & C Header Exporter

Trains a simple 784->64->10 fully connected classifier on MNIST,
quantizes weights to int8, and exports everything needed for the
C inference program as a single header file (mnist_model.h).

The exported model runs on the Ignytion Accelerator IP inside
a LiteX+VexRiscv virtual prototype in Renode.

Requirements:
    pip install torch torchvision numpy

Usage:
    python3 train_mnist.py                    # default output: mnist_model.h
    python3 train_mnist.py -o src/mnist_model.h
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torchvision import datasets, transforms
from torch.utils.data import DataLoader
import numpy as np
import argparse
import os
import sys


# -------------------------------------------------------------------
# Model Definition
# -------------------------------------------------------------------
class MNISTNet(nn.Module):
    """Minimal fully-connected MNIST classifier: 784 -> 64 -> 10"""
    def __init__(self):
        super(MNISTNet, self).__init__()
        self.fc1 = nn.Linear(784, 64)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(64, 10)

    def forward(self, x):
        x = x.view(-1, 784)
        x = self.relu(self.fc1(x))
        x = self.fc2(x)
        return x


# -------------------------------------------------------------------
# Training
# -------------------------------------------------------------------
def train_model(model, train_loader, epochs=10):
    optimizer = optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()

    for epoch in range(epochs):
        model.train()
        correct = 0
        total = 0
        total_loss = 0.0
        for data, target in train_loader:
            optimizer.zero_grad()
            output = model(data)
            loss = criterion(output, target)
            loss.backward()
            optimizer.step()
            pred = output.argmax(dim=1)
            correct += pred.eq(target).sum().item()
            total += len(target)
            total_loss += loss.item()
        acc = 100.0 * correct / total
        avg_loss = total_loss / len(train_loader)
        print("  Epoch {:2d}/{}  loss={:.4f}  acc={:.1f}%".format(
            epoch + 1, epochs, avg_loss, acc))

    return model


def evaluate_model(model, test_loader):
    model.eval()
    correct = 0
    total = 0
    with torch.no_grad():
        for data, target in test_loader:
            output = model(data)
            pred = output.argmax(dim=1)
            correct += pred.eq(target).sum().item()
            total += len(target)
    return 100.0 * correct / total


# -------------------------------------------------------------------
# Quantization
#
# Strategy:
#   - Input: raw pixel uint8 [0,255], pushed directly to accelerator
#   - Weights: int8 symmetric per-layer quantization
#   - Biases: int32, scaled to accumulator domain
#   - Layer 1 accumulator: int32 = sum(w1_q * pixel)
#     Max value: 784 * 127 * 255 ~ 25.4M (fits int32)
#   - Rescale between layers: right-shift by SHIFT1 bits
#   - ReLU: max(0, shifted_value)
#   - Layer 2: dot product of int8 weights x int32 hidden values
#   - Output: argmax (no rescaling needed)
# -------------------------------------------------------------------
def quantize_model(model, test_dataset):
    """Quantize model weights to int8 and compute all scaling parameters."""

    w1 = model.fc1.weight.data.numpy()   # (64, 784) float32
    b1 = model.fc1.bias.data.numpy()     # (64,) float32
    w2 = model.fc2.weight.data.numpy()   # (10, 64) float32
    b2 = model.fc2.bias.data.numpy()     # (10,) float32

    # --- Quantize weights to int8 (symmetric per-layer) ---
    w1_absmax = np.max(np.abs(w1))
    w1_scale = w1_absmax / 127.0
    w1_q = np.clip(np.round(w1 / w1_scale), -128, 127).astype(np.int8)

    w2_absmax = np.max(np.abs(w2))
    w2_scale = w2_absmax / 127.0
    w2_q = np.clip(np.round(w2 / w2_scale), -128, 127).astype(np.int8)

    print("  W1 scale: {:.6e}  (absmax={:.4f})".format(w1_scale, w1_absmax))
    print("  W2 scale: {:.6e}  (absmax={:.4f})".format(w2_scale, w2_absmax))

    # --- Layer 1 accumulator scale ---
    # The model was trained with input = pixel/255.0 (range [0,1])
    # For quantized inference, we push raw pixels (0-255) as int32
    # int_acc = sum(w1_q[i] * pixel[i])
    # float_equivalent = int_acc * w1_scale / 255.0
    # So: acc_to_float = w1_scale / 255.0

    acc_to_float = w1_scale / 255.0

    # --- Quantize Layer 1 bias ---
    # float_pre_act = int_acc * acc_to_float + b1
    # = (int_acc + b1_q) * acc_to_float
    # So: b1_q = round(b1 / acc_to_float)
    b1_q = np.round(b1 / acc_to_float).astype(np.int32)

    # --- Determine SHIFT1 empirically ---
    # Run quantized Layer 1 on samples to find max accumulator value,
    # then choose shift so hidden values fit in a safe range for Layer 2
    n_samples = min(1000, len(test_dataset))
    max_abs_acc = 0
    for i in range(n_samples):
        img, _ = test_dataset[i]
        pixels = (img.numpy().flatten() * 255).astype(np.uint8)
        int_acc = w1_q.astype(np.int32) @ pixels.astype(np.int32) + b1_q
        max_abs_acc = max(max_abs_acc, np.max(np.abs(int_acc)))

    # Target: max hidden value around 300-500 after shift
    # This keeps Layer 2 accumulator (64 * 127 * 500 ~ 4M) well within int32
    target_max_hidden = 500
    SHIFT1 = 0
    while (max_abs_acc >> SHIFT1) > target_max_hidden:
        SHIFT1 += 1

    print("  Max Layer 1 |accumulator|: {}".format(max_abs_acc))
    print("  SHIFT1 = {} (max hidden ~ {})".format(SHIFT1, max_abs_acc >> SHIFT1))

    # --- Quantize Layer 2 bias ---
    # hidden_q = max(0, (int_acc + b1_q) >> SHIFT1)
    # hidden_float = hidden_q * acc_to_float * (1 << SHIFT1)
    # = hidden_q * hidden_unit
    hidden_unit = acc_to_float * (1 << SHIFT1)

    # float_output = sum(W2 * hidden_float) + B2
    # = sum(w2_q * w2_scale * hidden_q * hidden_unit) + B2
    # = w2_scale * hidden_unit * int_l2_acc + B2
    # For argmax equivalence: int_l2_acc + b2_q
    # where b2_q = round(B2 / (w2_scale * hidden_unit))
    l2_total_scale = w2_scale * hidden_unit
    b2_q = np.round(b2 / l2_total_scale).astype(np.int32)

    print("  Hidden unit scale: {:.6e}".format(hidden_unit))
    print("  L2 total scale: {:.6e}".format(l2_total_scale))

    # --- Verify quantized inference accuracy ---
    correct = 0
    for i in range(n_samples):
        img, label = test_dataset[i]
        pixels = (img.numpy().flatten() * 255).astype(np.uint8)

        # Layer 1: dot product + bias + shift + ReLU
        int_acc = w1_q.astype(np.int32) @ pixels.astype(np.int32) + b1_q
        hidden = np.maximum(0, int_acc >> SHIFT1)

        # Layer 2: dot product + bias
        output = w2_q.astype(np.int32) @ hidden.astype(np.int32) + b2_q
        pred = np.argmax(output)

        if pred == label:
            correct += 1

    quant_acc = 100.0 * correct / n_samples
    print("  Quantized accuracy ({} samples): {:.1f}%".format(n_samples, quant_acc))

    return {
        'w1_q': w1_q, 'b1_q': b1_q,
        'w2_q': w2_q, 'b2_q': b2_q,
        'SHIFT1': SHIFT1,
        'w1_scale': w1_scale, 'w2_scale': w2_scale,
    }


# -------------------------------------------------------------------
# Select test images (one per digit, correctly classified)
# -------------------------------------------------------------------
def select_test_images(test_dataset, qp, n=10):
    w1_q, b1_q = qp['w1_q'], qp['b1_q']
    w2_q, b2_q = qp['w2_q'], qp['b2_q']
    SHIFT1 = qp['SHIFT1']

    selected = {}
    for i in range(len(test_dataset)):
        if len(selected) >= n:
            break

        img, label = test_dataset[i]
        label = int(label)
        if label in selected:
            continue

        pixels = (img.numpy().flatten() * 255).astype(np.uint8)

        # Quantized inference
        int_acc = w1_q.astype(np.int32) @ pixels.astype(np.int32) + b1_q
        hidden = np.maximum(0, int_acc >> SHIFT1)
        output = w2_q.astype(np.int32) @ hidden.astype(np.int32) + b2_q
        pred = int(np.argmax(output))

        if pred == label:
            selected[label] = pixels

    result_pixels = []
    result_labels = []
    for digit in sorted(selected.keys()):
        result_pixels.append(selected[digit])
        result_labels.append(digit)

    return result_pixels, result_labels


# -------------------------------------------------------------------
# Export C header
# -------------------------------------------------------------------
def export_c_header(qp, test_pixels, test_labels, output_path):
    w1_q, b1_q = qp['w1_q'], qp['b1_q']
    w2_q, b2_q = qp['w2_q'], qp['b2_q']
    SHIFT1 = qp['SHIFT1']

    with open(output_path, 'w') as f:

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
        f.write("/* -------------------------------------------------------------------\n")
        f.write(" * IGNYTION IO\n")
        f.write(" * Copyright (c) 2026 IGNYTION IO. All Rights Reserved.\n")
        f.write(" *\n")
        f.write(" * This software and associated documentation files are proprietary\n")
        f.write(" * and confidential. Unauthorized copying, modification, distribution,\n")
        f.write(" * or use of this file, via any medium, is strictly prohibited.\n")
        f.write(" *\n")
        f.write(" * Auto-generated MNIST model\n")
        f.write(" * Generated by train_mnist.py\n")
        f.write(" *\n")
        f.write(" *  Architecture: FC 784 -> 64 (ReLU) -> 10\n")
        f.write(" *  Quantization: int8 weights, int32 biases/accumulators\n")
        f.write(" *\n")
        f.write(" *  DO NOT EDIT -- regenerate with: python3 train_mnist.py\n")
        f.write(" * ------------------------------------------------------------------- */\n\n")
        f.write("#ifndef MNIST_MODEL_H\n")
        f.write("#define MNIST_MODEL_H\n\n")
        f.write("#include <stdint.h>\n\n")

        f.write("/* ---- Model dimensions ---- */\n")
        f.write("#define L1_IN_SIZE    784\n")
        f.write("#define L1_OUT_SIZE   64\n")
        f.write("#define L2_IN_SIZE    64\n")
        f.write("#define L2_OUT_SIZE   10\n\n")

        f.write("/* ---- Quantization parameters ---- */\n")
        f.write("#define L1_SHIFT      {}\n\n".format(SHIFT1))

        f.write("/* ---- Test data ---- */\n")
        f.write("#define NUM_TEST_IMAGES  {}\n".format(len(test_labels)))
        f.write("#define IMAGE_SIZE       784\n\n")

        # Layer 1 weights
        f.write("/* Layer 1 weights: {} x {} (int8) */\n".format(*w1_q.shape))
        f.write("static const int8_t l1_weights[L1_OUT_SIZE][L1_IN_SIZE] = {\n")
        for i in range(w1_q.shape[0]):
            vals = ", ".join(str(int(v)) for v in w1_q[i])
            comma = "," if i < w1_q.shape[0] - 1 else ""
            f.write("    {{ {} }}{}\n".format(vals, comma))
        f.write("};\n\n")

        # Layer 1 bias
        f.write("/* Layer 1 bias (int32) */\n")
        vals = ", ".join(str(int(v)) for v in b1_q)
        f.write("static const int32_t l1_bias[L1_OUT_SIZE] = {{ {} }};\n\n".format(vals))

        # Layer 2 weights
        f.write("/* Layer 2 weights: {} x {} (int8) */\n".format(*w2_q.shape))
        f.write("static const int8_t l2_weights[L2_OUT_SIZE][L2_IN_SIZE] = {\n")
        for i in range(w2_q.shape[0]):
            vals = ", ".join(str(int(v)) for v in w2_q[i])
            comma = "," if i < w2_q.shape[0] - 1 else ""
            f.write("    {{ {} }}{}\n".format(vals, comma))
        f.write("};\n\n")

        # Layer 2 bias
        f.write("/* Layer 2 bias (int32) */\n")
        vals = ", ".join(str(int(v)) for v in b2_q)
        f.write("static const int32_t l2_bias[L2_OUT_SIZE] = {{ {} }};\n\n".format(vals))

        # Test images
        f.write("/* Test images: {} x {} (uint8 pixels) */\n".format(
            len(test_pixels), 784))
        f.write("static const uint8_t test_images[NUM_TEST_IMAGES][IMAGE_SIZE] = {\n")
        for i, pixels in enumerate(test_pixels):
            vals = ", ".join(str(int(v)) for v in pixels)
            comma = "," if i < len(test_pixels) - 1 else ""
            f.write("    {{ {} }}{}\n".format(vals, comma))
        f.write("};\n\n")

        # Test labels
        f.write("/* Test labels (ground truth) */\n")
        vals = ", ".join(str(int(v)) for v in test_labels)
        f.write("static const uint8_t test_labels[NUM_TEST_IMAGES] = {{ {} }};\n\n".format(vals))

        f.write("#endif /* MNIST_MODEL_H */\n")

    size_kb = os.path.getsize(output_path) / 1024.0
    print("  Exported: {} ({:.1f} KB)".format(output_path, size_kb))


# -------------------------------------------------------------------
# Main
# -------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Train MNIST model and export quantized C header")
    parser.add_argument("--output", "-o", default="mnist_model.h",
                        help="Output C header path (default: mnist_model.h)")
    parser.add_argument("--epochs", type=int, default=10,
                        help="Training epochs (default: 10)")
    args = parser.parse_args()

    print("=" * 60)
    print("\tIGNYTION IO -- MNIST Model Trainer")
    print("=" * 60)

    # --- Load data ---
    print("\n[1] Loading MNIST dataset...")
    transform = transforms.ToTensor()
    train_dataset = datasets.MNIST('./data', train=True, download=True,
                                   transform=transform)
    test_dataset = datasets.MNIST('./data', train=False, download=True,
                                  transform=transform)
    train_loader = DataLoader(train_dataset, batch_size=64, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=1000)
    print("  Train: {} images  Test: {} images".format(
        len(train_dataset), len(test_dataset)))

    # --- Train ---
    print("\n[2] Training model ({} epochs)...".format(args.epochs))
    model = MNISTNet()
    model = train_model(model, train_loader, epochs=args.epochs)

    float_acc = evaluate_model(model, test_loader)
    print("\n  Float model test accuracy: {:.1f}%".format(float_acc))

    # --- Quantize ---
    print("\n[3] Quantizing to int8...")
    qp = quantize_model(model, test_dataset)

    # --- Select test images ---
    print("\n[4] Selecting test images...")
    test_pixels, test_labels = select_test_images(test_dataset, qp, n=10)
    print("  Selected {} images: digits {}".format(
        len(test_labels), test_labels))

    # --- Export ---
    print("\n[5] Exporting C header...")
    export_c_header(qp, test_pixels, test_labels, args.output)

    print("\n" + "=" * 60)
    print("  Done! Next steps:")
    print("  1. Copy {} to your project src/ directory".format(args.output))
    print("  2. Compile: riscv32-linux-gcc -static -O2 -o mnist_classify mnist_classify.c")
    print("  3. Update MAX_VECTOR_LEN to 1024 in ignytion_accel.py")
    print("  4. Inject binary into rootfs and boot in Renode")
    print("=" * 60)


if __name__ == "__main__":
    main()
