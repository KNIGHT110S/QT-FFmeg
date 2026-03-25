# Repository Guidelines

## Project Structure & Module Organization
The application is a small Qt Widgets desktop player built from the repository root. Core sources live beside the top-level `CMakeLists.txt`: `main.cpp`, `mainwindow.h`, `mainwindow.cpp`, and `mainwindow.ui`. Reference material and implementation notes live in `docs/`. Vendored FFmpeg headers, libraries, and runtime DLLs are expected under `third_party/FFmeg/` (or `third_party/ffmpeg/`). Treat `build/` and `build*/` as generated output from Qt Creator or CMake and do not commit contents from those directories.

## Build, Test, and Development Commands
Use CMake with a Windows Qt kit already installed.

```powershell
cmake -S . -B build\Desktop_Qt_6_9_1_MSVC2022_64bit-Debug
cmake --build build\Desktop_Qt_6_9_1_MSVC2022_64bit-Debug --config Debug
.\build\Desktop_Qt_6_9_1_MSVC2022_64bit-Debug\QT-FFmeg-player.exe
```

The configure step locates Qt Widgets and local FFmpeg binaries. The build step compiles the app and copies FFmpeg DLLs next to the executable. If you use MinGW instead of MSVC, keep the build directory name/toolchain consistent with that kit.

## Coding Style & Naming Conventions
Follow the existing Qt/C++ style in this repo: 4-space indentation, opening braces on the next line for functions, and initializer lists split across lines when needed. Keep class names in `PascalCase`, member functions in `camelCase`, and UI pointer members named clearly (`ui`). Prefer one class per paired header/source file, for example `playerwindow.h` and `playerwindow.cpp`. Let Qt's AUTOUIC/AUTOMOC generate `ui_*.h` and `moc_*.cpp`; never edit generated files.

## Testing Guidelines
There is no automated test suite yet. After code changes, do not run compile-based verification; perform logical validation by reviewing control flow, dependencies, and likely runtime effects instead. When adding non-trivial logic, introduce Qt Test or another CMake-integrated test target and name files after the unit under test, such as `test_mainwindow.cpp`.

## Commit & Pull Request Guidelines
The branch currently has no commit history, so no repository-specific convention exists yet. Use short, imperative commit subjects such as `Add FFmpeg library detection`. Keep pull requests focused and include: a brief summary, build/test notes, linked issue if applicable, and screenshots for `.ui` or visual changes.

When the user confirms changes should be committed/pushed to the remote repository, push to `https://github.com/KNIGHT110S/QT-FFmeg.git` (do not push anywhere else, and do not push without explicit user confirmation).

## Agent-Specific Instructions
If the user's goal or requested behavior is unclear, stop and ask clarifying questions before making code changes. Do not choose a direction independently when the requirement is ambiguous.
