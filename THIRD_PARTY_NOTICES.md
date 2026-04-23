# Уведомления о сторонних компонентах

OCB Studio Native интегрирует выбранные сторонние исходники.

## UEFITool

- Upstream: https://github.com/LongSoft/UEFITool
- Расположение: `tools/uefitool/upstream`
- Лицензия: лицензия в стиле BSD, сохранена в `tools/uefitool/upstream/LICENSE.md`

## Dear ImGui

- Upstream: https://github.com/ocornut/imgui
- Получение: CMake `FetchContent`
- Лицензия: MIT

## GLFW

- Upstream: https://github.com/glfw/glfw
- Получение: CMake `FetchContent`
- Лицензия: zlib/libpng

Universal IFR Extractor больше не включен в исходное дерево и не используется при сборке. Извлечение HII/IFR выполняет собственная реализация в `tools/ifr/src/NativeIfrExtractor.cpp`.
