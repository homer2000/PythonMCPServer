# Contributing to PythonMCPServer

Thank you for your interest in contributing!

## How to Contribute

### 1. Fork and Clone

```bash
git clone https://github.com/your-username/PythonMCPServer.git
cd PythonMCPServer
```

### 2. Create a Branch

```bash
git checkout -b feature/your-feature-name
```

### 3. Build and Test

```bash
mkdir build
cd build
/opt/Qt/5.15.2/gcc_64/bin/qmake ..
make -j$(nproc)
```

### 4. Commit and Push

```bash
git add .
git commit -m "feat: your feature description"
git push origin feature/your-feature-name
```

### 5. Create a Pull Request

Submit a pull request with a clear description of your changes.

## Development Setup

- **Qt**: 5.15.2 or later, or Qt 6.x
- **Compiler**: GCC, Clang, or MSVC with C++17 support
- **Python**: 3.6+

## Project Structure

```
├── src/           # Source files
├── include/       # Public headers
├── docs/          # Documentation
├── tests/         # Unit tests
├── PythonMCPServer.pro
└── README.md
```

## License

By contributing, you agree that your contributions will be licensed under the MIT License.