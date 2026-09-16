# lcmp

A Modern C++ software to parse and compare material lists between two Minecraft `.litematic` schematic files.

It acts like `git diff` for `.litematic` files, making it easy to track block count changes (e.g., checking block savings or additions after optimizing redstone circuitry or compacting structures).

> But it doesn't act like [schematic-diff](https://github.com/Arcadi4/schematic-diff), lcmp just compares the materials' delta. 

## Requirements

- **CPU Support:** Requires x86_64 CPU with **AVX2** and **BMI2** instruction sets support.

## Features

- Parse block material counts from `.litematic` files and calculate material differences (`delta`, `base_count`, `target_count`) between two files.

- High-performance SIMD parsing built exclusively for AVX2 & BMI2 architecture.

- Provides a lightweight GUI built with Qt Quick 6.8.

## How to use

Drag the base `.litematic` file to left, target `.litematic` file to right.

If you want to use `lcmp.h` directly, make sure to set up its required dependencies in your own build system.

## License

lcmp is licensed under the **GNU General Public License v3.0 only (GPL-3.0-only)**.

## Dependencies

- [Google Abseil](https://github.com/abseil/abseil-cpp) (Apache-2.0)

- [pdqsort](https://github.com/orlp/pdqsort) (zlib)

- [zlib](https://github.com/madler/zlib) (zlib)

- [libnbt++](https://github.com/PrismLauncher/libnbtplusplus) (GPL-3.0)
