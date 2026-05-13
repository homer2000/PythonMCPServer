# Configuration

## MCP Client Setup

Configure PythonMCPServer in your MCP-compatible client.

### Claude Code

Add to your MCP settings:

```json
{
  "mcpServers": {
    "python": {
      "command": "/full/path/to/PythonMCPServer"
    }
  }
}
```

### Claude Desktop (claude.ai)

Add to `%APPDATA%\\Claude\\claude_desktop_config.json` (Windows) or `~/Library/Application Support/Claude/claude_desktop_config.json` (macOS):

```json
{
  "mcpServers": {
    "python": {
      "command": "/full/path/to/PythonMCPServer"
    }
  }
}
```

## Server Options

The server runs in stdio mode by default. No additional configuration files are required.

### Environment Variables

| Variable | Description | Default |
|---|---|---|
| `PYTHON_HOME` | Path to Python installation | System Python |
| `WORK_DIR` | Working directory for scripts | Current directory |

## Tool Reference

### Script Management

| Tool | Parameters |
|---|---|
| `python_create_script` | `name`, `code` |
| `python_list_scripts` | None |
| `python_read_script` | `name` |
| `python_delete_script` | `name` |
| `python_run_script` | `name`, `args` (optional) |

### Environment Management

| Tool | Parameters |
|---|---|
| `python_create_env` | `name` |
| `python_delete_env` | `name` |
| `python_activate_env` | `name` |
| `python_list_envs` | None |

### Package Management

| Tool | Parameters |
|---|---|
| `python_install_package` | `packages` (space-separated) |
| `python_install_requirements` | `file` |
| `python_create_requirements` | None |
| `python_list_packages` | `env` (optional) |

### Execution

| Tool | Parameters |
|---|---|
| `python_execute` | `code` |
| `python_debug_script` | `name` |
| `python_profile_script` | `name` |