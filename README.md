# mfm
MFM emulator code

This code is for a MFM hard drive reader and emulator.
Project home page is at http://www.pdp8online.com/mfm/

## Building

### Requirements

- meson (>= 1.3.0)
- ninja
- git
- C compiler with C99 support
- C++ compiler with C++11 support
- libm
- librt
- pthread
- libiberty

Meson will refuse to configure the project if you are missing any dependencies.

### Build Instructions

```bash
# Update git submodules
git submodule update --init --recursive

# Configure the build (downloads wrap dependencies automatically)
meson setup build

# Compile the project
meson compile -C build

# Run tests
meson test -C build
```

### Code Coverage

To generate code coverage reports:

```bash
# Configure with coverage enabled
meson setup build --buildtype=debug -Db_coverage=true

# Compile and test
meson compile -C build
meson test -C build

# Generate coverage report
ninja coverage -C build
```

The coverage report will be available in `build/meson-logs/coveragereport/`.
