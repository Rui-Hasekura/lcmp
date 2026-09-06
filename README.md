# lcmp

A Modern C++ library to parse and compare material lists between two Minecraft `.litematic` schematic files.

It acts like `git diff` for `.litematic` files, making it easy to track block count changes (e.g., checking block savings or additions after optimizing redstone circuitry or compacting structures).

## Features

- Parse block material counts from `.litematic` files.

- Calculate material differences (`delta`, `base_count`, `new_count`) between two files.

- Extract basic schematic metadata (author, name, volume, total block count).

## License

lcmp is licensed under the **GNU General Public License v3.0 only (GPL-3.0-only)**.

## Dependencies

- [Google Abseil](https://github.com/abseil/abseil-cpp)

- [pdqsort](https://github.com/orlp/pdqsort)

- [zlib](https://github.com/madler/zlib)

- [libnbt++](https://github.com/PrismLauncher/libnbtplusplus)
