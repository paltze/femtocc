# C Compiler & Assembler Toolchain for Femto-8

An ISO C compiler (`cc`) and assembler (`as`) targeting [Femto-8](https://github.com/paltze/femto)

## Prerequisites

* C Compiler: GCC or Clang (with ASan/UBSan support for debug builds)
* Build Tools: CMake (3.20+), Ninja, GNU Make
* Optional: `clang-format` (for code formatting)

## Quick Start

All common workflows are wrapped in the root `Makefile`:

```bash
# Configure and build cc and as
make build

# Compile a C source file into a binary
./build/cc program.c -o program.hex

# Emit assembly only
./build/cc program.c -S -o program.asm

# Run the standalone assembler
./build/as program.asm -o program.hex
```

## Development Commands

| Command | Description |
| :--- | :--- |
| `make build` | Configures CMake with Ninja (ASan + UBSan enabled) and compiles `cc` and `as` into `build/`. |
| `make format` | Formats all project `.c` and `.h` files in place using `clang-format`. |
| `make check-format` | Verifies formatting without modifying files (exits non-zero on drift). |
| `make clean` | Removes the `build/` directory. |

## License
This project is licensed under the MIT License. See the LICENSE file for details.
