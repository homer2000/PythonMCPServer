# PythonMCPServer

MCP-сервер (stdio) на C++/Qt для создания, управления и запуска Python-скриптов из Claude Code и других MCP-клиентов. Поддерживает виртуальные окружения, pip-пакеты, отладку и профилирование.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CI](https://github.com/python-mcp-server/PythonMCPServer/workflows/CI/badge.svg)](https://github.com/python-mcp-server/PythonMCPServer/actions)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Qt](https://img.shields.io/badge/Qt-5.15.2%20%7C%206.x-41CD3A.svg)](https://qt.io)

MCP-сервер (stdio transport) на C++/Qt для создания, управления и запуска Python-скриптов из Claude Code или любого другого MCP-клиента.

## Features

### Script Management

| Tool | Description |
|---|---|
| `python_create_script` | Create or overwrite a `.py` file |
| `python_run_script` | Run a script (from global or venv Python) |
| `python_list_scripts` | List scripts in the working directory |
| `python_read_script` | Read script contents |
| `python_delete_script` | Delete a script |

### Environment Management

| Tool | Description |
|---|---|
| `python_create_env` | Create a virtual environment (venv) |
| `python_delete_env` | Delete a virtual environment |
| `python_activate_env` | Activate environment for subsequent commands |
| `python_list_envs` | List created venv environments |
| `python_list_packages` | List installed packages in environment |

### Package Management

| Tool | Description |
|---|---|
| `python_install_package` | Install package(s) via pip |
| `python_install_requirements` | Install dependencies from requirements.txt |
| `python_create_requirements` | Create requirements.txt from environment |
| `python_export_env` | Export environment as configuration |
| `python_import_env` | Import environment from configuration |

### Execution & Debugging

| Tool | Description |
|---|---|
| `python_execute` | Execute Python code directly (REPL) |
| `python_debug_script` | Run script in debug mode |
| `python_profile_script` | Profile script execution |

## Project Structure

```
├── src/           # Source files
├── include/       # Public headers
├── docs/          # Documentation
├── tests/         # Unit tests
└── PythonMCPServer.pro
```

## Building

### Using qmake

```bash
mkdir build && cd build
/opt/Qt/5.15.2/gcc_64/bin/qmake ..
make -j$(nproc)
```

### Using Qt Creator

1. Open `PythonMCPServer.pro` in Qt Creator
2. Build → Run qmake
3. Build → Build All

## Requirements

- Qt 5.15.2+ or Qt 6.x
- C++17 compiler (GCC, Clang, or MSVC)
- Python 3.6+ (for script execution)

## Usage

Configure the server in your MCP client (e.g., Claude Code):

```json
{
  "mcpServers": {
    "python": {
      "command": "/path/to/PythonMCPServer"
    }
  }
}
```

## Documentation

- [Building from Source](docs/building.md)
- [Configuration](docs/configuration.md)
- [API Reference](docs/api.md)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup and pull request guidelines.

## License

MIT License - see [LICENSE](LICENSE) for details.