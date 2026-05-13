# Building PythonMCPServer

## Prerequisites

- **Qt**: 5.15.2 or later (recommended), or Qt 6.x
- **Compiler**: GCC 7+, Clang 6+, or MSVC 2019+
- **C++17**: Required standard support

## Build Options

### Using qmake

```bash
mkdir build
cd build
/opt/Qt/5.15.2/gcc_64/bin/qmake ..
make -j$(nproc)
```

### Using Qt Creator

1. Open `PythonMCPServer.pro` in Qt Creator
2. Configure Kit (Qt version)
3. Build → Run qmake
4. Build → Build All

## Platform-Specific Notes

### Linux

- Install Qt development packages: `sudo apt install qtbase5-dev`

### Windows

- Use Qt Online Installer
- Select "MinGW 64-bit" or "MSVC 2019 64-bit"

### macOS

- Install Qt from qt.io or Homebrew: `brew install qt@5`

## Output

After successful build:
- **Linux/macOS**: `build/PythonMCPServer`
- **Windows**: `build/PythonMCPServer.exe`