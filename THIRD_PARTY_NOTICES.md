# Уведомления о сторонних компонентах

OCB Studio Native не включает исходники внешних BIOS/IFR-экстракторов. Разбор firmware и HII/IFR выполняется собственной реализацией в `tools/uefi` и `tools/ifr`.

## Dear ImGui

- Upstream: https://github.com/ocornut/imgui
- Получение: CMake `FetchContent`
- Лицензия: MIT

## GLFW

- Upstream: https://github.com/glfw/glfw
- Получение: CMake `FetchContent`
- Лицензия: zlib/libpng

## LZMA SDK decoder

- Upstream: https://www.7-zip.org/sdk.html
- Расположение: `tools/uefi/third_party/lzma`
- Лицензия: public domain

Внешние BIOS/IFR-экстракторы больше не включены в исходное дерево и не используются при сборке.
