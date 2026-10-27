# Virtual Prototyping MNIST Classifier

A Renode virtual prototype running a quantized MNIST handwritten-digit classifier
on LiteX/VexRiscv Linux, with a custom accelerator model, functional tests, and
documentation. No physical hardware is required.

All supported host execution uses the Igny CLI: workspace setup, simulator
launch, and optional cross-compilation.

The accelerator is a Python behavioral peripheral mapped at `0xF0010000`.
The classifier runs on the simulated RISC-V CPU and sends every neural-network
dot product to that peripheral through memory-mapped registers.

## Included demonstrations

| Program in the guest | What it does |
|---|---|
| `/root/mnist_classify` | Classifies ten embedded digit images, with ASCII previews |
| `/root/mnist_classify -q` | Runs the same classifier with compact output |
| `/root/test_accel` | Checks unsigned and signed dot products, elementwise addition, and ReLU |
| `/root/ignytion_banner` | Displays the Ignytion banner and guest platform information |
| `/root/hello_world_ignytion` | Runs a RISC-V Linux hello-world program |

All five commands use this one virtual platform. The executable programs are
already included in `binaries/custom_rootfs/rootfs_copy`; training and
cross-compilation are optional.

## Run with Igny on Windows

Install and initialize the Igny CLI so `igny` is available in PowerShell. From the
repository root:

```powershell
igny workspace sync
igny run renode --console scripts/litex_vexriscv_accel.resc -e s --hide-log
```

`crucible.toml` selects the `mnist-demo` environment. `crucible.lock` pins
**Renode 1.16.1 for windows-x86_64**, including its artifact identity and SHA-256.
Sync acquires and binds that tool if it is missing.

When the UART window shows `buildroot login:`, enter `root`. The demonstration
guest has no root password. Then run:

```sh
./hello_world_ignytion
./ignytion_banner
./test_accel
./mnist_classify
```

These are guest programs: they execute on the simulated RISC-V CPU inside the
virtual prototype launched through Igny. The Renode Monitor is a separate
console. Close Renode after the demonstration; guest filesystem changes are
temporary.

The supported launch command is `igny run renode`. The supplied lock is for
Windows. Linux execution requires a separately provisioned Igny environment and
a Linux-specific lock; that path was not rerun during this release preparation.

## Model and accelerator

- Network: fully connected `784 -> 64 (ReLU) -> 10`.
- Quantization: int8 weights and integer accumulators.
- Test inputs: ten selected images embedded in `src/mnist_model.h`.
- Accelerator CONFIG: vector length in bits `15:0`; operation mode in bits `23:16`.
- Operations: dot product (`0`), elementwise addition (`1`), and ReLU (`2`).

The ten images were selected from cases the quantized model classifies correctly.
An observed `10/10` result describes this demonstration set, not accuracy over the
full MNIST test dataset. The accelerator cycle counter is a modeled count, not a
cycle-accurate timing result or measured hardware performance. ReLU between
classifier layers runs in C on the simulated CPU.

## Verified run

Verified on **27 October 2026**, using native Windows Renode 1.16.1 through
`igny run`. UART output and the guest exit status were checked for each program.

| Check | Observed result |
|---|---|
| Hello world | Expected message; exit `0` |
| Banner | Reports `Linux-riscv32`; exit `0` |
| Accelerator tests | Four checks passed; exit `0` |
| MNIST classifier | Ten correct predictions out of ten; exit `0` |
| Accelerator calls | `740` |
| Modeled accelerator cycles | `1,016,320` |

The packaged accelerator test was rebuilt from the current source with Buildroot
GCC 14.3.0 (`riscv32-gcc` package `2025.08-1`) and replaced inside the rootfs.
This corrects the old executable's CONFIG encoding for addition and ReLU.
The runtime lock contains only Renode; the optional build compiler is separate.

The minimal Buildroot guest emits mount and network-service warnings during
boot. The four programs above completed successfully in that guest.

## Repository layout

```text
.
|-- binaries/
|   |-- bios, dtb, kernel
|   |-- custom_rootfs/rootfs_copy
|   `-- hello_world_ignytion, ignytion_banner, test_accel, mnist_classify
|-- docs/Virtual_Prototyping_MNIST_Classifier_Whitepaper.pdf
|-- platforms/cpus/litex_vexriscv_accel.repl
|-- scripts/litex_vexriscv_accel.resc
|-- src/
|-- crucible.toml
`-- crucible.lock
```

`src/ignytion_accel.py` is required at runtime. The other sources are supplied for
rebuilding or retraining. The original training dataset is not bundled.

## Optional rebuild through Igny

The rebuild uses a RISC-V **Linux** cross-compiler acquired and run through Igny.
In an initialized Igny shell on Linux/WSL, from the repository root, create a
separate build environment and install the compiler:

```sh
igny env create mnist-build
igny compiler install riscv32 --env mnist-build
```

If that environment already exists, use it. Build the programs with:

```sh
igny run --env mnist-build riscv32-buildroot-linux-musl-gcc -static -O2 -Wl,--strip-debug -o binaries/test_accel src/test_accel.c
igny run --env mnist-build riscv32-buildroot-linux-musl-gcc -static -O2 -o binaries/mnist_classify src/mnist_classify.c
igny run --env mnist-build riscv32-buildroot-linux-musl-gcc -static -O2 -o binaries/ignytion_banner src/banner.c
igny run --env mnist-build riscv32-buildroot-linux-musl-gcc -static -O2 -o binaries/hello_world_ignytion src/hello_world_ignytion.c
```

This optional build environment is separate from the supplied Windows runtime
lock, which contains only Renode.

To use a rebuilt executable in the guest, replace the matching `/root/` file in
the uncompressed `newc` CPIO archive
`binaries/custom_rootfs/rootfs_copy`, preserving its executable permissions.
Updating only the loose binary in `binaries/` does not update the running guest.

`src/train_mnist.py` is retained as the model-generation source. Retraining
requires additional dependencies that are not provisioned by the supplied Igny
runtime. The supported classifier workflow uses the included model header and
guest executable through the Igny-managed virtual prototype.

## Documentation

Read the [whitepaper](docs/Virtual_Prototyping_MNIST_Classifier_Whitepaper.pdf)
for the broader virtual-prototyping demonstration. The repository README is the
current guide for this standalone package. The PDF retains its original research
narrative; its local host screenshots and workspace instructions have been
replaced with portable examples, and document metadata has been cleaned.

Copyright 2026 Ignytion IO. Existing source notices apply.
