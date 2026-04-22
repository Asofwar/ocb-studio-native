# Contributing

Thanks for taking the time to improve OCB Studio Native.

## Development Principles

- Keep the application native C++ and Qt.
- Prefer small, reviewable changes over broad rewrites.
- Do not add external runtime tools when source integration or a clean library wrapper is practical.
- Keep firmware handling conservative and explicit.
- Do not commit user BIOS images, OCB profiles, serial numbers, board dumps, or other machine-specific artifacts.

## Local Build

Core/tools only:

```powershell
cmake -S . -B build -DOCB_BUILD_UI=OFF -DOCB_BUILD_TESTS=OFF
cmake --build build --config Release --parallel
```

Full Qt app:

```powershell
cmake -S . -B build-ui -DOCB_BUILD_UI=ON -DOCB_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=C:\Qt\6.6.3\msvc2019_64
cmake --build build-ui --config Release --parallel
```

## Pull Requests

Before opening a pull request:

- Build the affected targets.
- Run tests when you have the required local fixtures.
- Update documentation for user-visible behavior.
- Explain firmware safety implications for changes that affect parsing, mapping, checksums, or saved output.

## Coding Style

- C++20 for project code.
- CMake targets should remain modular.
- Public headers live under `include/ocb/...`.
- Use clear C++ interfaces around vendored tools instead of exposing upstream internals broadly.
