# Contributing

Thanks for helping improve OCB Studio Native.

## Principles

- Keep the project native C++ with Dear ImGui/GLFW built as static libraries.
- Prefer small, reviewable changes over broad rewrites.
- Do not add external runtime tools when a local C++ wrapper or integrated source path is practical.
- Keep firmware handling conservative and explicit.
- Do not commit user BIOS images, OCB profiles, serial numbers, board dumps, or other machine-specific artifacts.

## Local Build

```powershell
cmake -S . -B build -DOCB_BUILD_APP=ON -DOCB_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

On Linux, install X11/OpenGL development packages for GLFW before configuring.

For tools/core only:

```powershell
cmake -S . -B build-core -DOCB_BUILD_APP=OFF -DOCB_BUILD_TESTS=OFF
cmake --build build-core --config Release --parallel
```

## Pull Requests

Before opening a pull request:

- Build the affected targets.
- Run tests when the required local fixture files are available.
- Update documentation for user-visible behavior.
- Explain firmware safety implications when changes affect parsing, mapping, checksums, or saved output.

## Code Style

- Use C++20 for project code.
- Keep CMake targets modular.
- Put public headers under `include/ocb/...`.
- Use clear C++ interfaces around vendored tools instead of exposing broad upstream internals.
