# OCB Studio Native

[![CI](https://github.com/Asofwar/ocb-studio-native/actions/workflows/ci.yml/badge.svg)](https://github.com/Asofwar/ocb-studio-native/actions/workflows/ci.yml)
[![Лицензия: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

OCB Studio Native - настольный инструмент на C++20 и Dear ImGui, CLI-режим и библиотека для просмотра и редактирования файлов профилей разгона MSI (`MsOcFile.ocb`). Приложение умеет применять встроенные пресеты, импортировать и экспортировать файлы пресетов, записывать отдельные значения полей, компенсировать контрольные суммы и расширять каталог полей на основе BIOS IFR.

Проект намеренно не зависит от Qt, Electron или webview. Графический интерфейс построен на Dear ImGui, GLFW и OpenGL; CMake загружает и собирает Dear ImGui/GLFW как статические библиотеки. Разбор firmware, извлечение PE32-модулей и чтение HII/IFR выполняются собственными нативными C++-парсерами без внешних BIOS/IFR-экстракторов.

## Возможности

- Загрузка, проверка, редактирование и сохранение MSI OCB-профилей.
- Применение встроенных пресетов и пресетов из `.ocbpreset` / JSON-файлов.
- Экспорт встроенных пресетов в переносимые файлы пресетов.
- Запись отдельных значений по идентификатору поля или текстовой подсказке.
- Компенсация контрольных сумм OCB для выходных файлов, принимаемых BIOS.
- Анализ BIOS-образов через встроенный конвейер UEFI/IFR.
- Сборка через CMake с компилятором C++20, системным toolchain и статически связанными Dear ImGui/GLFW.

## Безопасность

Изменения firmware и параметров разгона могут сделать систему нестабильной или незагружаемой. Считайте сгенерированные профили экспериментальными, храните проверенные резервные копии и применяйте только те изменения, которые понимаете. Проект предоставляет инструменты, но не гарантирует, что конкретная плата, версия firmware или профиль примет отредактированный файл.

## Структура

```text
app/      исполняемый файл Dear ImGui, CLI-режим и контроллер приложения
core/     модель OCB-профиля, поля, пресеты, файлы пресетов, контрольные суммы, анализ BIOS
tools/    C++-обертки и нативные парсеры для firmware/IFR
tests/    нативный тестовый исполняемый файл
```

## Требования

- CMake 3.24 или новее.
- Компилятор с поддержкой C++20:
  - MSVC 2022 на Windows.
  - Актуальный Clang или GCC на Linux.
  - AppleClang на macOS.

Qt SDK не требуется. Приложение использует Dear ImGui `v1.92.7` и GLFW `3.4` через CMake `FetchContent`.

## Сборка

```powershell
cmake -S . -B build -DOCB_BUILD_APP=ON -DOCB_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Сборка только библиотеки и инструментов:

```powershell
cmake -S . -B build-core -DOCB_BUILD_APP=OFF -DOCB_BUILD_TESTS=OFF
cmake --build build-core --config Release --parallel
```

Сборки MSVC по умолчанию используют статический runtime (`/MT`) через `OCB_STATIC_MSVC_RUNTIME=ON`, поэтому релизным бинарным файлам не нужны DLL Visual C++ runtime рядом с ними.

На Linux перед конфигурацией установите обычные пакеты разработки X11/OpenGL для GLFW, например `xorg-dev`, `libglu1-mesa-dev` и `pkg-config` на Ubuntu.

## Использование GUI

Запустите исполняемый файл без аргументов:

```powershell
.\build\app\Release\ocb_studio.exe
```

Интерфейс Dear ImGui позволяет загружать файлы OCB/BIOS/IFR, сохранять OCB, компенсировать контрольные суммы, импортировать и экспортировать пресеты, применять пресеты, искать поля и напрямую редактировать значения.

## Использование CLI

Показать список встроенных пресетов:

```powershell
.\build\app\Release\ocb_studio.exe --list-presets
```

Экспортировать встроенный пресет:

```powershell
.\build\app\Release\ocb_studio.exe --export-preset "Консервативный 200/220W 307A" --output conservative.ocbpreset
```

Применить встроенный пресет:

```powershell
.\build\app\Release\ocb_studio.exe --input MsOcFile.ocb --output MsOcFile.patched.ocb --preset "Консервативный 200/220W 307A"
```

Применить импортированный файл пресета:

```powershell
.\build\app\Release\ocb_studio.exe --input MsOcFile.ocb --output MsOcFile.patched.ocb --preset-file conservative.ocbpreset
```

Записать одно поле:

```powershell
.\build\app\Release\ocb_studio.exe --input MsOcFile.ocb --output MsOcFile.patched.ocb --write "CPU Lite Load" 30
```

Добавьте `--no-compensate`, если нужен сырой вывод без компенсации контрольной суммы.

## Формат файла пресета

Файл пресета - это JSON-объект:

```json
{
  "format": "OCB Studio Preset",
  "version": 1,
  "name": "Example",
  "values": {
    "Long Duration Power Limit (W)": 200,
    "Short Duration Power Limit (W)": 220,
    "CPU Lite Load": "0x1E"
  }
}
```

Значения могут быть неотрицательными десятичными целыми числами или строками с десятичными/шестнадцатеричными числами.

## Тесты

Некоторые интеграционные проверки зависят от локальных fixture-файлов BIOS/OCB. Публичный CI собирает исходные цели без проприетарных fixture-данных.

```powershell
cmake -S . -B build-test -DOCB_BUILD_APP=ON -DOCB_BUILD_TESTS=ON
cmake --build build-test --config Release --parallel
ctest --test-dir build-test -C Release --output-on-failure
```

## Сторонние исходники

Репозиторий включает выбранные фрагменты исходников из:

Dear ImGui и GLFW загружаются через CMake `FetchContent`. Внешние BIOS/IFR-экстракторы не входят в дерево исходников и не используются при сборке.
