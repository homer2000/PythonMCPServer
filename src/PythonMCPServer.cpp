#include "PythonMCPServer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

PythonMCPServer::PythonMCPServer(QObject *parent)
    : QMCPServer("PythonMCPServer", "2.0.0", parent)
{
    setWorkDir(QDir::currentPath());

    setInstructions(
        "Python Script Manager — создаёт, запускает и управляет Python-скриптами. "
        "Поддерживает виртуальные окружения (venv), установку пакетов через pip, "
        "прямое выполнение кода (REPL), отладку, профилирование и мониторинг памяти. "
        "Все скрипты хранятся в рабочей директории сервера; "
        "окружения — в поддиректории envs/."
    );

    // ── Управление скриптами ─────────────────────────────────────────────────

    registerTool({
        "python_create_script",
        "Создать или перезаписать Python-скрипт в рабочей директории сервера.",
        {
            { "name",     "string", "Имя файла скрипта (например, hello.py). "
                                    "Расширение .py добавляется автоматически.", true  },
            { "code",     "string", "Исходный код Python-скрипта.", true  },
            { "overwrite","string", "\"true\" — перезаписать существующий файл (по умолчанию false).", false }
        },
        "text"
    });

    registerTool({
        "python_run_script",
        "Запустить Python-скрипт. Возвращает stdout, stderr и код возврата.",
        {
            { "name",     "string", "Имя скрипта для запуска.", true  },
            { "args",     "string", "Аргументы командной строки (необязательно).", false },
            { "env_name", "string", "Имя venv-окружения (необязательно; если не указано — используется активное или системное).", false },
            { "timeout",  "string", "Таймаут выполнения в секундах (по умолчанию 30).", false }
        },
        "text"
    });

    registerTool({
        "python_list_scripts",
        "Показать список Python-скриптов в рабочей директории сервера.",
        {},
        "json"
    });

    registerTool({
        "python_read_script",
        "Прочитать содержимое Python-скрипта.",
        {
            { "name", "string", "Имя скрипта.", true }
        },
        "text"
    });

    registerTool({
        "python_delete_script",
        "Удалить Python-скрипт из рабочей директории.",
        {
            { "name", "string", "Имя скрипта для удаления.", true }
        },
        "text"
    });

    // ── Управление окружениями ───────────────────────────────────────────────

    registerTool({
        "python_create_env",
        "Создать виртуальное Python-окружение (venv) в директории envs/<name>.",
        {
            { "name",        "string", "Имя окружения (например, myenv).", true  },
            { "python_path", "string", "Путь к интерпретатору Python (необязательно).", false },
            { "recreate",    "string", "\"true\" — удалить и пересоздать окружение.", false }
        },
        "text"
    });

    registerTool({
        "python_delete_env",
        "Удалить виртуальное окружение и все его файлы.",
        {
            { "name",    "string", "Имя окружения для удаления.", true  },
            { "confirm", "string", "\"true\" — подтверждение удаления (обязательно).", true  }
        },
        "text"
    });

    registerTool({
        "python_activate_env",
        "Активировать окружение для последующих команд (python_run_script, python_execute и др.). "
        "Передайте пустую строку для сброса к системному Python.",
        {
            { "name", "string", "Имя окружения для активации. Пустая строка — деактивировать.", true }
        },
        "text"
    });

    registerTool({
        "python_list_envs",
        "Показать список созданных venv-окружений.",
        {},
        "json"
    });

    registerTool({
        "python_list_packages",
        "Список установленных пакетов в указанном окружении (или системном).",
        {
            { "env_name", "string", "Имя окружения (необязательно).", false },
            { "format",   "string", "Формат вывода: \"json\" или \"text\" (по умолчанию json).", false }
        },
        "json"
    });

    // ── Управление пакетами ──────────────────────────────────────────────────

    registerTool({
        "python_install_package",
        "Установить один или несколько pip-пакетов в указанное окружение.",
        {
            { "packages",  "string", "Список пакетов через пробел или запятую.", true  },
            { "env_name",  "string", "Имя venv-окружения (необязательно).", false },
            { "extra_args","string", "Дополнительные аргументы pip (например, \"--upgrade\").", false },
            { "timeout",   "string", "Таймаут в секундах (по умолчанию 60).", false }
        },
        "text"
    });

    registerTool({
        "python_install_requirements",
        "Установить зависимости из файла requirements.txt.",
        {
            { "file",     "string", "Путь к файлу requirements.txt (абсолютный или относительно workDir). "
                                    "По умолчанию — requirements.txt в workDir.", false },
            { "env_name", "string", "Имя venv-окружения (необязательно).", false },
            { "timeout",  "string", "Таймаут в секундах (по умолчанию 120).", false }
        },
        "text"
    });

    registerTool({
        "python_create_requirements",
        "Создать файл requirements.txt из текущего окружения (pip freeze).",
        {
            { "env_name", "string", "Имя окружения (необязательно).", false },
            { "output",   "string", "Путь для сохранения файла (по умолчанию workDir/requirements.txt).", false }
        },
        "text"
    });

    registerTool({
        "python_export_env",
        "Экспортировать конфигурацию окружения (pip freeze + метаданные) в JSON-файл.",
        {
            { "env_name", "string", "Имя окружения для экспорта.", true  },
            { "output",   "string", "Путь для сохранения .json-файла (необязательно).", false }
        },
        "text"
    });

    registerTool({
        "python_import_env",
        "Создать окружение и установить пакеты из ранее экспортированного JSON-файла.",
        {
            { "file",        "string", "Путь к .json-файлу конфигурации (экспортированному python_export_env).", true  },
            { "env_name",    "string", "Имя нового окружения (если не указано — берётся из файла).", false },
            { "python_path", "string", "Путь к интерпретатору Python для создания окружения (необязательно).", false }
        },
        "text"
    });

    // ── Выполнение и отладка ─────────────────────────────────────────────────

    registerTool({
        "python_execute",
        "Выполнить произвольный Python-код напрямую (без сохранения в файл). Аналог REPL.",
        {
            { "code",     "string", "Python-код для выполнения.", true  },
            { "env_name", "string", "Имя окружения (необязательно).", false },
            { "timeout",  "string", "Таймаут в секундах (по умолчанию 30).", false }
        },
        "text"
    });

    registerTool({
        "python_debug_script",
        "Запустить скрипт в режиме отладки (python -m pdb). "
        "Полезно для статического анализа; интерактивная отладка недоступна через MCP.",
        {
            { "name",     "string", "Имя скрипта.", true  },
            { "args",     "string", "Аргументы скрипта (необязательно).", false },
            { "env_name", "string", "Имя окружения (необязательно).", false },
            { "timeout",  "string", "Таймаут в секундах (по умолчанию 30).", false }
        },
        "text"
    });

    registerTool({
        "python_profile_script",
        "Профилировать скрипт с помощью cProfile и вернуть топ-N строк статистики.",
        {
            { "name",     "string", "Имя скрипта.", true  },
            { "args",     "string", "Аргументы скрипта (необязательно).", false },
            { "env_name", "string", "Имя окружения (необязательно).", false },
            { "top",      "string", "Количество строк в отчёте (по умолчанию 20).", false },
            { "sort",     "string", "Поле сортировки: cumulative|time|calls|name (по умолчанию cumulative).", false },
            { "timeout",  "string", "Таймаут в секундах (по умолчанию 120).", false }
        },
        "text"
    });

    // ── Работа с файлами ─────────────────────────────────────────────────────

    registerTool({
        "python_read_file",
        "Прочитать произвольный текстовый файл (не обязательно .py) в рабочей директории.",
        {
            { "path",     "string", "Путь к файлу (относительно workDir или абсолютный внутри workDir).", true  },
            { "encoding", "string", "Кодировка файла (по умолчанию UTF-8).", false }
        },
        "text"
    });

    registerTool({
        "python_write_file",
        "Записать текст/код в произвольный файл в рабочей директории.",
        {
            { "path",      "string", "Путь к файлу (относительно workDir).", true  },
            { "content",   "string", "Содержимое файла.", true  },
            { "overwrite", "string", "\"true\" — перезаписать существующий файл (по умолчанию false).", false },
            { "encoding",  "string", "Кодировка файла (по умолчанию UTF-8).", false }
        },
        "text"
    });

    registerTool({
        "python_find_imports",
        "Найти все import/from...import в Python-скрипте и вернуть список модулей.",
        {
            { "name", "string", "Имя скрипта для анализа.", true }
        },
        "json"
    });

    // ── Мониторинг ───────────────────────────────────────────────────────────

    registerTool({
        "python_memory_usage",
        "Запустить скрипт через tracemalloc и показать использование памяти.",
        {
            { "name",     "string", "Имя скрипта.", true  },
            { "args",     "string", "Аргументы скрипта (необязательно).", false },
            { "env_name", "string", "Имя окружения (необязательно).", false },
            { "top",      "string", "Топ-N строк по использованию памяти (по умолчанию 10).", false },
            { "timeout",  "string", "Таймаут в секундах (по умолчанию 60).", false }
        },
        "text"
    });

    registerTool({
        "python_process_info",
        "Получить информацию о доступных Python-интерпретаторах и активном окружении.",
        {
            { "env_name", "string", "Имя окружения для проверки (необязательно).", false }
        },
        "json"
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// setWorkDir
// ─────────────────────────────────────────────────────────────────────────────

void PythonMCPServer::setWorkDir(const QString &path)
{
    m_workDir = QDir::cleanPath(path);
    m_envsDir = m_workDir + "/envs";

    QDir().mkpath(m_workDir);
    QDir().mkpath(m_envsDir);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dispatch
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::handleToolCall(const QString &name,
                                              const QJsonObject &args)
{
    // Управление скриптами
    if (name == "python_create_script")        return toolCreateScript(args);
    if (name == "python_run_script")           return toolRunScript(args);
    if (name == "python_list_scripts")         return toolListScripts(args);
    if (name == "python_read_script")          return toolReadScript(args);
    if (name == "python_delete_script")        return toolDeleteScript(args);

    // Управление окружениями
    if (name == "python_create_env")           return toolCreateEnv(args);
    if (name == "python_delete_env")           return toolDeleteEnv(args);
    if (name == "python_activate_env")         return toolActivateEnv(args);
    if (name == "python_list_envs")            return toolListEnvs(args);
    if (name == "python_list_packages")        return toolListPackages(args);

    // Управление пакетами
    if (name == "python_install_package")      return toolInstallPackage(args);
    if (name == "python_install_requirements") return toolInstallRequirements(args);
    if (name == "python_create_requirements")  return toolCreateRequirements(args);
    if (name == "python_export_env")           return toolExportEnv(args);
    if (name == "python_import_env")           return toolImportEnv(args);

    // Выполнение и отладка
    if (name == "python_execute")              return toolExecute(args);
    if (name == "python_debug_script")         return toolDebugScript(args);
    if (name == "python_profile_script")       return toolProfileScript(args);

    // Работа с файлами
    if (name == "python_read_file")            return toolReadFile(args);
    if (name == "python_write_file")           return toolWriteFile(args);
    if (name == "python_find_imports")         return toolFindImports(args);

    // Мониторинг
    if (name == "python_memory_usage")         return toolMemoryUsage(args);
    if (name == "python_process_info")         return toolProcessInfo(args);

    return MCPToolResult::error("Unknown tool: " + name);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_create_script
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolCreateScript(const QJsonObject &args)
{
    QString name = args.value("name").toString().trimmed();
    if (name.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (!name.endsWith(".py", Qt::CaseInsensitive))
        name += ".py";

    const QString code      = args.value("code").toString();
    const bool    overwrite = args.value("overwrite").toString().toLower() == "true";

    QString err;
    const QString path = scriptPath(name, &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    if (QFile::exists(path) && !overwrite)
        return MCPToolResult::error(
            QString("Script '%1' already exists. Pass overwrite=true to replace it.").arg(name));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return MCPToolResult::error("Cannot write file: " + file.errorString());

    QTextStream ts(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ts.setEncoding(QStringConverter::Utf8);
#else
    ts.setCodec("UTF-8");
#endif
    ts << code;
    file.close();

    return MCPToolResult::ok(
        QString("Script '%1' created successfully at %2").arg(name, path));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_run_script
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolRunScript(const QJsonObject &args)
{
    const QString name    = args.value("name").toString().trimmed();
    QString       envName = args.value("env_name").toString().trimmed();
    const QString argStr  = args.value("args").toString().trimmed();
    const int     timeout = args.value("timeout").toString().toInt() > 0
                            ? args.value("timeout").toString().toInt() * 1000
                            : kRunTimeoutMs;

    if (name.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    // Если окружение не указано явно — использовать активное
    if (envName.isEmpty())
        envName = m_activeEnv;

    QString err;
    const QString path = scriptPath(name.endsWith(".py") ? name : name + ".py", &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    if (!QFile::exists(path))
        return MCPToolResult::error(QString("Script '%1' not found.").arg(name));

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "Python interpreter not found. Install Python and make sure it is in PATH."
            : QString("venv '%1' not found or Python not available in it.").arg(envName));

    QStringList cmdArgs { path };
    if (!argStr.isEmpty())
        cmdArgs += argStr.split(' ', Qt::SkipEmptyParts);

    dbg(QString("[PythonMCPServer] Running: %1 %2").arg(python, cmdArgs.join(' ')));
    return runProcess(python, cmdArgs, timeout, m_workDir);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_list_scripts
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolListScripts(const QJsonObject &)
{
    QDir dir(m_workDir);
    const QStringList files = dir.entryList({ "*.py" }, QDir::Files, QDir::Name);

    QJsonArray arr;
    for (const QString &f : files) {
        QFileInfo fi(dir.filePath(f));
        arr.append(QJsonObject {
            { "name",     f },
            { "size",     fi.size() },
            { "modified", fi.lastModified().toString(Qt::ISODate) }
        });
    }

    QJsonObject result { { "scripts", arr }, { "workDir", m_workDir } };
    return MCPToolResult::okJson(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_read_script
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolReadScript(const QJsonObject &args)
{
    QString name = args.value("name").toString().trimmed();
    if (name.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (!name.endsWith(".py", Qt::CaseInsensitive))
        name += ".py";

    QString err;
    const QString path = scriptPath(name, &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    QFile file(path);
    if (!file.exists())
        return MCPToolResult::error(QString("Script '%1' not found.").arg(name));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return MCPToolResult::error("Cannot read file: " + file.errorString());

    return MCPToolResult::ok(QString::fromUtf8(file.readAll()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_delete_script
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolDeleteScript(const QJsonObject &args)
{
    QString name = args.value("name").toString().trimmed();
    if (name.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (!name.endsWith(".py", Qt::CaseInsensitive))
        name += ".py";

    QString err;
    const QString path = scriptPath(name, &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    if (!QFile::exists(path))
        return MCPToolResult::error(QString("Script '%1' not found.").arg(name));

    if (!QFile::remove(path))
        return MCPToolResult::error(QString("Failed to delete '%1'.").arg(name));

    return MCPToolResult::ok(QString("Script '%1' deleted.").arg(name));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_create_env
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolCreateEnv(const QJsonObject &args)
{
    const QString envName    = args.value("name").toString().trimmed();
    const QString pythonPath = args.value("python_path").toString().trimmed();
    const bool    recreate   = args.value("recreate").toString().toLower() == "true";

    if (envName.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (!isSafeFileName(envName))
        return MCPToolResult::error("Environment name contains invalid characters.");

    const QString envDir = envPath(envName);

    if (QDir(envDir).exists()) {
        if (!recreate)
            return MCPToolResult::error(
                QString("Environment '%1' already exists. Pass recreate=true to replace it.")
                .arg(envName));

        if (!QDir(envDir).removeRecursively())
            return MCPToolResult::error(
                QString("Cannot remove existing environment at '%1'.").arg(envDir));
    }

    QString python = pythonPath;
    if (python.isEmpty()) {
        for (const QString candidate : { "python3", "python" }) {
            QProcess p;
            p.start(candidate, { "--version" });
            if (p.waitForFinished(3000) && p.exitCode() == 0) {
                python = candidate;
                break;
            }
        }
    }
    if (python.isEmpty())
        return MCPToolResult::error("Python interpreter not found in PATH.");

    sendLog(QString("Creating venv '%1' using %2...").arg(envName, python));

    MCPToolResult res = runProcess(python, { "-m", "venv", envDir }, kDefaultTimeoutMs);
    if (res.isError)
        return res;

    return MCPToolResult::ok(
        QString("Virtual environment '%1' created at %2\n\n%3")
        .arg(envName, envDir, res.text));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_delete_env
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolDeleteEnv(const QJsonObject &args)
{
    const QString envName = args.value("name").toString().trimmed();
    const bool    confirm = args.value("confirm").toString().toLower() == "true";

    if (envName.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (!confirm)
        return MCPToolResult::error(
            "Pass confirm=true to confirm deletion of the virtual environment.");

    if (!isSafeFileName(envName))
        return MCPToolResult::error("Environment name contains invalid characters.");

    const QString envDir = envPath(envName);

    if (!QDir(envDir).exists())
        return MCPToolResult::error(
            QString("Environment '%1' not found at '%2'.").arg(envName, envDir));

    // Если удаляемое окружение активно — сбросить активацию
    if (m_activeEnv == envName) {
        m_activeEnv.clear();
        sendLog(QString("Active environment '%1' was deactivated before deletion.").arg(envName));
    }

    if (!QDir(envDir).removeRecursively())
        return MCPToolResult::error(
            QString("Failed to delete environment '%1' at '%2'.").arg(envName, envDir));

    return MCPToolResult::ok(
        QString("Virtual environment '%1' deleted from '%2'.").arg(envName, envDir));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_activate_env
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolActivateEnv(const QJsonObject &args)
{
    const QString envName = args.value("name").toString().trimmed();

    if (envName.isEmpty()) {
        m_activeEnv.clear();
        return MCPToolResult::ok("Active environment cleared. System Python will be used.");
    }

    if (!isSafeFileName(envName))
        return MCPToolResult::error("Environment name contains invalid characters.");

    const QString envDir = envPath(envName);
    if (!QDir(envDir).exists())
        return MCPToolResult::error(
            QString("Environment '%1' not found. Create it first with python_create_env.").arg(envName));

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            QString("Python executable not found in environment '%1'. The environment may be corrupted.").arg(envName));

    m_activeEnv = envName;

    return MCPToolResult::ok(
        QString("Environment '%1' activated. All subsequent commands will use it by default.\n"
                "Python: %2").arg(envName, python));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_list_envs
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolListEnvs(const QJsonObject &)
{
    QDir envsDir(m_envsDir);
    const QStringList dirs = envsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QJsonArray arr;
    for (const QString &d : dirs) {
        const QString cfgPath = m_envsDir + "/" + d + "/pyvenv.cfg";
        if (!QFile::exists(cfgPath))
            continue;

        QString pythonVersion;
        QFile cfg(cfgPath);
        if (cfg.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!cfg.atEnd()) {
                const QString line = QString::fromUtf8(cfg.readLine()).trimmed();
                if (line.startsWith("version")) {
                    pythonVersion = line.section('=', 1).trimmed();
                    break;
                }
            }
        }

        QFileInfo fi(m_envsDir + "/" + d);
        arr.append(QJsonObject {
            { "name",          d           },
            { "path",          fi.absoluteFilePath() },
            { "pythonVersion", pythonVersion },
            { "active",        d == m_activeEnv },
            { "created",       fi.birthTime().isValid()
                                ? fi.birthTime().toString(Qt::ISODate)
                                : fi.lastModified().toString(Qt::ISODate) }
        });
    }

    QJsonObject result {
        { "environments", arr },
        { "envsDir",      m_envsDir },
        { "activeEnv",    m_activeEnv.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(m_activeEnv) }
    };
    return MCPToolResult::okJson(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_list_packages
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolListPackages(const QJsonObject &args)
{
    QString       envName = args.value("env_name").toString().trimmed();
    const QString format  = args.value("format").toString().trimmed().toLower();

    if (envName.isEmpty())
        envName = m_activeEnv;

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "Python interpreter not found."
            : QString("Environment '%1' not found or Python not available.").arg(envName));

    // Используем pip list --format json для машиночитаемого вывода
    MCPToolResult res = runProcess(python, { "-m", "pip", "list", "--format", "json" },
                                   kDefaultTimeoutMs);
    if (res.isError)
        return res;

    // Извлечь JSON из вывода (убрать заголовок Exit code)
    const QString rawJson = res.text.section('\n', 1).trimmed();

    if (format == "text") {
        // Возвращаем текстовый pip list
        MCPToolResult textRes = runProcess(python, { "-m", "pip", "list" }, kDefaultTimeoutMs);
        return textRes;
    }

    QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8());
    if (doc.isNull()) {
        // Fallback: вернуть как текст
        return MCPToolResult::ok(res.text);
    }

    QJsonObject result {
        { "packages",    doc.array() },
        { "environment", envName.isEmpty() ? "system" : envName },
        { "count",       doc.array().size() }
    };
    return MCPToolResult::okJson(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_install_package
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolInstallPackage(const QJsonObject &args)
{
    const QString packagesStr = args.value("packages").toString().trimmed();
    QString       envName     = args.value("env_name").toString().trimmed();
    const QString extraArgs   = args.value("extra_args").toString().trimmed();
    const int     timeout     = args.value("timeout").toString().toInt() > 0
                                ? args.value("timeout").toString().toInt() * 1000
                                : kDefaultTimeoutMs;

    if (packagesStr.isEmpty())
        return MCPToolResult::error("Parameter 'packages' is required.");

    if (envName.isEmpty())
        envName = m_activeEnv;

    const QString pip = pipExecutable(envName);
    if (pip.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "pip not found. Install pip or use a virtual environment."
            : QString("venv '%1' not found or pip not available in it.").arg(envName));

    QStringList packages = packagesStr.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);

    QStringList pipArgs { "install" };
    pipArgs += packages;
    if (!extraArgs.isEmpty())
        pipArgs += extraArgs.split(' ', Qt::SkipEmptyParts);

    sendLog(QString("Installing: %1%2")
        .arg(packages.join(", "),
             envName.isEmpty() ? "" : QString(" (in env '%1')").arg(envName)));

    return runProcess(pip, pipArgs, timeout);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_install_requirements
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolInstallRequirements(const QJsonObject &args)
{
    QString reqFile = args.value("file").toString().trimmed();
    QString envName = args.value("env_name").toString().trimmed();
    const int timeout = args.value("timeout").toString().toInt() > 0
                        ? args.value("timeout").toString().toInt() * 1000
                        : kDefaultTimeoutMs * 2;

    if (envName.isEmpty())
        envName = m_activeEnv;

    // Определить путь к requirements.txt
    if (reqFile.isEmpty())
        reqFile = m_workDir + "/requirements.txt";
    else if (!QFileInfo(reqFile).isAbsolute())
        reqFile = QDir::cleanPath(m_workDir + "/" + reqFile);

    if (!QFile::exists(reqFile))
        return MCPToolResult::error(
            QString("Requirements file not found: %1").arg(reqFile));

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "Python interpreter not found."
            : QString("Environment '%1' not found.").arg(envName));

    sendLog(QString("Installing requirements from %1%2")
        .arg(reqFile,
             envName.isEmpty() ? "" : QString(" (in env '%1')").arg(envName)));

    return runProcess(python, { "-m", "pip", "install", "-r", reqFile }, timeout);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_create_requirements
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolCreateRequirements(const QJsonObject &args)
{
    QString envName = args.value("env_name").toString().trimmed();
    QString output  = args.value("output").toString().trimmed();

    if (envName.isEmpty())
        envName = m_activeEnv;

    if (output.isEmpty())
        output = m_workDir + "/requirements.txt";
    else if (!QFileInfo(output).isAbsolute())
        output = QDir::cleanPath(m_workDir + "/" + output);

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "Python interpreter not found."
            : QString("Environment '%1' not found.").arg(envName));

    MCPToolResult res = runProcess(python, { "-m", "pip", "freeze" }, kDefaultTimeoutMs);
    if (res.isError)
        return res;

    // Извлечь содержимое (без заголовка Exit code)
    const QString content = res.text.section('\n', 1).trimmed();

    QFile file(output);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return MCPToolResult::error("Cannot write requirements file: " + file.errorString());

    file.write(content.toUtf8());
    file.close();

    const int pkgCount = content.isEmpty() ? 0 : content.split('\n', Qt::SkipEmptyParts).size();
    return MCPToolResult::ok(
        QString("requirements.txt created at %1\n%2 packages listed.")
        .arg(output).arg(pkgCount));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_export_env
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolExportEnv(const QJsonObject &args)
{
    const QString envName = args.value("env_name").toString().trimmed();
    QString       output  = args.value("output").toString().trimmed();

    if (envName.isEmpty())
        return MCPToolResult::error("Parameter 'env_name' is required.");

    if (!isSafeFileName(envName))
        return MCPToolResult::error("Environment name contains invalid characters.");

    const QString envDir = envPath(envName);
    if (!QDir(envDir).exists())
        return MCPToolResult::error(
            QString("Environment '%1' not found.").arg(envName));

    if (output.isEmpty())
        output = m_workDir + "/" + envName + "_env.json";
    else if (!QFileInfo(output).isAbsolute())
        output = QDir::cleanPath(m_workDir + "/" + output);

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            QString("Python not available in environment '%1'.").arg(envName));

    // Получить pip freeze
    MCPToolResult freezeRes = runProcess(python, { "-m", "pip", "freeze" }, kDefaultTimeoutMs);
    const QString freezeOutput = freezeRes.text.section('\n', 1).trimmed();

    // Получить версию Python
    MCPToolResult verRes = runProcess(python, { "--version" }, 5000);
    const QString pyVersion = verRes.text.section('\n', 1).trimmed()
                                         .remove("Python ").trimmed();

    // Прочитать pyvenv.cfg
    QString basePrefix;
    QFile cfg(envDir + "/pyvenv.cfg");
    if (cfg.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!cfg.atEnd()) {
            const QString line = QString::fromUtf8(cfg.readLine()).trimmed();
            if (line.startsWith("base-prefix") || line.startsWith("base_prefix")) {
                basePrefix = line.section('=', 1).trimmed();
                break;
            }
        }
    }

    QJsonArray packagesArr;
    for (const QString &pkg : freezeOutput.split('\n', Qt::SkipEmptyParts))
        packagesArr.append(pkg.trimmed());

    QJsonObject exportObj {
        { "name",          envName },
        { "pythonVersion", pyVersion },
        { "basePrefix",    basePrefix },
        { "packages",      packagesArr },
        { "exportedAt",    QDateTime::currentDateTime().toString(Qt::ISODate) }
    };

    QFile outFile(output);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return MCPToolResult::error("Cannot write export file: " + outFile.errorString());

    outFile.write(QJsonDocument(exportObj).toJson(QJsonDocument::Indented));
    outFile.close();

    return MCPToolResult::ok(
        QString("Environment '%1' exported to %2\n%3 packages.")
        .arg(envName, output).arg(packagesArr.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_import_env
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolImportEnv(const QJsonObject &args)
{
    QString       file       = args.value("file").toString().trimmed();
    QString       envName    = args.value("env_name").toString().trimmed();
    const QString pythonPath = args.value("python_path").toString().trimmed();

    if (file.isEmpty())
        return MCPToolResult::error("Parameter 'file' is required.");

    if (!QFileInfo(file).isAbsolute())
        file = QDir::cleanPath(m_workDir + "/" + file);

    if (!QFile::exists(file))
        return MCPToolResult::error(QString("Configuration file not found: %1").arg(file));

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return MCPToolResult::error("Cannot read configuration file: " + f.errorString());

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseErr);
    f.close();

    if (doc.isNull())
        return MCPToolResult::error("Invalid JSON in configuration file: " + parseErr.errorString());

    const QJsonObject config = doc.object();

    if (envName.isEmpty())
        envName = config.value("name").toString().trimmed();

    if (envName.isEmpty())
        return MCPToolResult::error("Cannot determine environment name from file or parameter.");

    if (!isSafeFileName(envName))
        return MCPToolResult::error("Environment name contains invalid characters.");

    // Создать окружение
    QJsonObject createArgs;
    createArgs["name"] = envName;
    if (!pythonPath.isEmpty())
        createArgs["python_path"] = pythonPath;
    createArgs["recreate"] = "false";

    MCPToolResult createRes = toolCreateEnv(createArgs);
    if (createRes.isError)
        return createRes;

    // Установить пакеты
    const QJsonArray packages = config.value("packages").toArray();
    if (packages.isEmpty())
        return MCPToolResult::ok(
            QString("Environment '%1' imported (no packages to install).\n\n%2")
            .arg(envName, createRes.text));

    QStringList pkgList;
    for (const QJsonValue &v : packages)
        pkgList.append(v.toString());

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            QString("Python not available in newly created environment '%1'.").arg(envName));

    QStringList pipArgs { "-m", "pip", "install" };
    pipArgs += pkgList;

    sendLog(QString("Importing %1 packages into environment '%2'...").arg(pkgList.size()).arg(envName));

    MCPToolResult installRes = runProcess(python, pipArgs, kDefaultTimeoutMs * 3);

    return MCPToolResult::ok(
        QString("Environment '%1' imported from %2\n%3 packages installed.\n\n%4")
        .arg(envName, file).arg(pkgList.size())
        .arg(installRes.text));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_execute
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolExecute(const QJsonObject &args)
{
    const QString code    = args.value("code").toString();
    QString       envName = args.value("env_name").toString().trimmed();
    const int     timeout = args.value("timeout").toString().toInt() > 0
                            ? args.value("timeout").toString().toInt() * 1000
                            : kRunTimeoutMs;

    if (code.isEmpty())
        return MCPToolResult::error("Parameter 'code' is required.");

    if (envName.isEmpty())
        envName = m_activeEnv;

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "Python interpreter not found."
            : QString("Environment '%1' not found.").arg(envName));

    return runProcess(python, { "-c", code }, timeout, m_workDir);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_debug_script
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolDebugScript(const QJsonObject &args)
{
    const QString name    = args.value("name").toString().trimmed();
    QString       envName = args.value("env_name").toString().trimmed();
    const QString argStr  = args.value("args").toString().trimmed();
    const int     timeout = args.value("timeout").toString().toInt() > 0
                            ? args.value("timeout").toString().toInt() * 1000
                            : kRunTimeoutMs;

    if (name.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (envName.isEmpty())
        envName = m_activeEnv;

    QString err;
    const QString path = scriptPath(name.endsWith(".py") ? name : name + ".py", &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    if (!QFile::exists(path))
        return MCPToolResult::error(QString("Script '%1' not found.").arg(name));

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "Python interpreter not found."
            : QString("Environment '%1' not found.").arg(envName));

    // Запускаем через -m pdb с флагом -c 'continue' для неинтерактивного режима
    // Это позволяет получить трассировку ошибок с номерами строк
    QStringList cmdArgs { "-m", "pdb", "-c", "continue", path };
    if (!argStr.isEmpty())
        cmdArgs += argStr.split(' ', Qt::SkipEmptyParts);

    return runProcess(python, cmdArgs, timeout, m_workDir);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_profile_script
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolProfileScript(const QJsonObject &args)
{
    const QString name    = args.value("name").toString().trimmed();
    QString       envName = args.value("env_name").toString().trimmed();
    const QString argStr  = args.value("args").toString().trimmed();
    const int     top     = args.value("top").toString().toInt() > 0
                            ? args.value("top").toString().toInt() : 20;
    const QString sort    = args.value("sort").toString().trimmed().isEmpty()
                            ? "cumulative" : args.value("sort").toString().trimmed();
    const int     timeout = args.value("timeout").toString().toInt() > 0
                            ? args.value("timeout").toString().toInt() * 1000
                            : kProfileTimeoutMs;

    if (name.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (envName.isEmpty())
        envName = m_activeEnv;

    QString err;
    const QString path = scriptPath(name.endsWith(".py") ? name : name + ".py", &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    if (!QFile::exists(path))
        return MCPToolResult::error(QString("Script '%1' not found.").arg(name));

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "Python interpreter not found."
            : QString("Environment '%1' not found.").arg(envName));

    // Генерируем обёртку для cProfile
    const QString profileCode = QString(
        "import cProfile, pstats, io, sys\n"
        "sys.argv = [%1] + sys.argv[1:]\n"
        "pr = cProfile.Profile()\n"
        "pr.enable()\n"
        "with open(%1, 'r') as _f:\n"
        "    exec(compile(_f.read(), %1, 'exec'), {'__name__': '__main__', '__file__': %1})\n"
        "pr.disable()\n"
        "s = io.StringIO()\n"
        "ps = pstats.Stats(pr, stream=s).sort_stats('%2')\n"
        "ps.print_stats(%3)\n"
        "print(s.getvalue())\n"
    ).arg(
        "'" + QString(path).replace("'", "\\\'") + "'",
        sort,
        QString::number(top)
    );

    QStringList cmdArgs { "-c", profileCode };
    if (!argStr.isEmpty())
        cmdArgs += argStr.split(' ', Qt::SkipEmptyParts);

    return runProcess(python, cmdArgs, timeout, m_workDir);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_read_file
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolReadFile(const QJsonObject &args)
{
    const QString rawPath  = args.value("path").toString().trimmed();
    const QString encoding = args.value("encoding").toString().trimmed();

    if (rawPath.isEmpty())
        return MCPToolResult::error("Parameter 'path' is required.");

    QString err;
    const QString path = safePath(rawPath, &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    QFile file(path);
    if (!file.exists())
        return MCPToolResult::error(QString("File not found: %1").arg(path));

    if (!file.open(QIODevice::ReadOnly))
        return MCPToolResult::error("Cannot read file: " + file.errorString());

    const QByteArray raw = file.readAll();
    file.close();

    QString content;
    if (encoding.isEmpty() || encoding.toLower() == "utf-8" || encoding.toLower() == "utf8") {
        content = QString::fromUtf8(raw);
    } else if (encoding.toLower() == "latin-1" || encoding.toLower() == "latin1") {
        content = QString::fromLatin1(raw);
    } else {
        content = QString::fromUtf8(raw); // fallback
    }

    return MCPToolResult::ok(content);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_write_file
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolWriteFile(const QJsonObject &args)
{
    const QString rawPath  = args.value("path").toString().trimmed();
    const QString content  = args.value("content").toString();
    const bool    overwrite= args.value("overwrite").toString().toLower() == "true";
    const QString encoding = args.value("encoding").toString().trimmed();

    if (rawPath.isEmpty())
        return MCPToolResult::error("Parameter 'path' is required.");

    QString err;
    const QString path = safePath(rawPath, &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    if (QFile::exists(path) && !overwrite)
        return MCPToolResult::error(
            QString("File '%1' already exists. Pass overwrite=true to replace it.").arg(path));

    // Создать родительские директории если нужно
    QDir().mkpath(QFileInfo(path).dir().absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return MCPToolResult::error("Cannot write file: " + file.errorString());

    if (encoding.toLower() == "latin-1" || encoding.toLower() == "latin1") {
        file.write(content.toLatin1());
    } else {
        file.write(content.toUtf8());
    }
    file.close();

    return MCPToolResult::ok(
        QString("File written successfully: %1 (%2 bytes)").arg(path).arg(content.toUtf8().size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_find_imports
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolFindImports(const QJsonObject &args)
{
    QString name = args.value("name").toString().trimmed();
    if (name.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (!name.endsWith(".py", Qt::CaseInsensitive))
        name += ".py";

    QString err;
    const QString path = scriptPath(name, &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    QFile file(path);
    if (!file.exists())
        return MCPToolResult::error(QString("Script '%1' not found.").arg(name));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return MCPToolResult::error("Cannot read file: " + file.errorString());

    const QString source = QString::fromUtf8(file.readAll());
    file.close();

    // Регулярные выражения для import и from...import
    static const QRegularExpression reImport(
        R"(^\s*import\s+([\w\s,\.]+))",
        QRegularExpression::MultilineOption);
    static const QRegularExpression reFrom(
        R"(^\s*from\s+([\w\.]+)\s+import\s+)",
        QRegularExpression::MultilineOption);

    QSet<QString> modules;

    auto it = reImport.globalMatch(source);
    while (it.hasNext()) {
        const auto m = it.next();
        const QStringList parts = m.captured(1).split(',');
        for (const QString &p : parts) {
            const QString mod = p.trimmed().split('.').first().split(' ').first();
            if (!mod.isEmpty())
                modules.insert(mod);
        }
    }

    auto it2 = reFrom.globalMatch(source);
    while (it2.hasNext()) {
        const auto m = it2.next();
        const QString mod = m.captured(1).trimmed().split('.').first();
        if (!mod.isEmpty())
            modules.insert(mod);
    }

    QStringList sortedModules = modules.values();
    sortedModules.sort(Qt::CaseInsensitive);

    QJsonArray arr;
    for (const QString &mod : sortedModules)
        arr.append(mod);

    QJsonObject result {
        { "script",  name },
        { "imports", arr  },
        { "count",   arr.size() }
    };
    return MCPToolResult::okJson(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_memory_usage
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolMemoryUsage(const QJsonObject &args)
{
    const QString name    = args.value("name").toString().trimmed();
    QString       envName = args.value("env_name").toString().trimmed();
    const QString argStr  = args.value("args").toString().trimmed();
    const int     top     = args.value("top").toString().toInt() > 0
                            ? args.value("top").toString().toInt() : 10;
    const int     timeout = args.value("timeout").toString().toInt() > 0
                            ? args.value("timeout").toString().toInt() * 1000
                            : kDefaultTimeoutMs * 2;

    if (name.isEmpty())
        return MCPToolResult::error("Parameter 'name' is required.");

    if (envName.isEmpty())
        envName = m_activeEnv;

    QString err;
    const QString path = scriptPath(name.endsWith(".py") ? name : name + ".py", &err);
    if (!err.isEmpty())
        return MCPToolResult::error(err);

    if (!QFile::exists(path))
        return MCPToolResult::error(QString("Script '%1' not found.").arg(name));

    const QString python = pythonExecutable(envName);
    if (python.isEmpty())
        return MCPToolResult::error(
            envName.isEmpty()
            ? "Python interpreter not found."
            : QString("Environment '%1' not found.").arg(envName));

    // Обёртка с tracemalloc
    const QString memCode = QString(
        "import tracemalloc, sys\n"
        "sys.argv = [%1] + sys.argv[1:]\n"
        "tracemalloc.start()\n"
        "_snap_before = tracemalloc.take_snapshot()\n"
        "with open(%1, 'r') as _f:\n"
        "    exec(compile(_f.read(), %1, 'exec'), {'__name__': '__main__', '__file__': %1})\n"
        "_snap_after = tracemalloc.take_snapshot()\n"
        "_stats = _snap_after.compare_to(_snap_before, 'lineno')\n"
        "print('\\n=== Memory usage (tracemalloc) top %2 ===')\n"
        "for _s in _stats[:%2]:\n"
        "    print(_s)\n"
        "_cur, _peak = tracemalloc.get_traced_memory()\n"
        "print(f'\\nCurrent: {_cur/1024:.1f} KB  Peak: {_peak/1024:.1f} KB')\n"
        "tracemalloc.stop()\n"
    ).arg(
        "'" + QString(path).replace("'", "\\\'") + "'",
        QString::number(top)
    );

    QStringList cmdArgs { "-c", memCode };
    if (!argStr.isEmpty())
        cmdArgs += argStr.split(' ', Qt::SkipEmptyParts);

    return runProcess(python, cmdArgs, timeout, m_workDir);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: python_process_info
// ─────────────────────────────────────────────────────────────────────────────

MCPToolResult PythonMCPServer::toolProcessInfo(const QJsonObject &args)
{
    const QString envName = args.value("env_name").toString().trimmed();

    QJsonObject result;

    // Активное окружение
    result["activeEnv"] = m_activeEnv.isEmpty()
                          ? QJsonValue(QJsonValue::Null)
                          : QJsonValue(m_activeEnv);

    // Проверяемое окружение (или активное/системное)
    const QString checkEnv = envName.isEmpty() ? m_activeEnv : envName;
    const QString python   = pythonExecutable(checkEnv);

    if (python.isEmpty()) {
        result["error"] = checkEnv.isEmpty()
            ? "No Python interpreter found in PATH."
            : QString("Python not found in environment '%1'.").arg(checkEnv);
        return MCPToolResult::okJson(result);
    }

    result["pythonExecutable"] = python;
    result["environment"]      = checkEnv.isEmpty() ? "system" : checkEnv;

    // Версия Python
    {
        QProcess p;
        p.start(python, { "--version" });
        if (p.waitForFinished(5000))
            result["pythonVersion"] = QString::fromUtf8(p.readAllStandardOutput()
                                        + p.readAllStandardError()).trimmed();
    }

    // sys.prefix, sys.exec_prefix, platform
    {
        const QString infoCode =
            "import sys, platform, json\n"
            "print(json.dumps({"
            "'prefix': sys.prefix,"
            "'exec_prefix': sys.exec_prefix,"
            "'platform': platform.platform(),"
            "'architecture': platform.architecture()[0],"
            "'sysPath': sys.path"
            "}))";

        QProcess p;
        p.start(python, { "-c", infoCode });
        if (p.waitForFinished(5000)) {
            const QByteArray out = p.readAllStandardOutput().trimmed();
            QJsonParseError pe;
            const QJsonDocument doc = QJsonDocument::fromJson(out, &pe);
            if (!doc.isNull())
                result["sysInfo"] = doc.object();
        }
    }

    // pip версия
    {
        const QString pip = pipExecutable(checkEnv);
        if (!pip.isEmpty()) {
            QProcess p;
            QString realPip = pip;
            QStringList pipArgs;
            if (pip.endsWith(" -m pip (use runProcess override)")) {
                realPip = pip.section(' ', 0, 0);
                pipArgs = QStringList{ "-m", "pip", "--version" };
            } else {
                pipArgs = QStringList{ "--version" };
            }
            p.start(realPip, pipArgs);
            if (p.waitForFinished(5000))
                result["pipVersion"] = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        }
    }

    result["workDir"] = m_workDir;
    result["envsDir"] = m_envsDir;

    return MCPToolResult::okJson(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

QString PythonMCPServer::scriptPath(const QString &name, QString *error) const
{
    if (!isSafeFileName(name)) {
        if (error) *error = QString("Invalid script name '%1'. "
                                    "Use only letters, digits, '_', '-', '.'").arg(name);
        return {};
    }

    const QString full = QDir::cleanPath(m_workDir + "/" + name);
    if (!full.startsWith(m_workDir + "/") && full != m_workDir) {
        if (error) *error = "Path traversal detected.";
        return {};
    }
    return full;
}

QString PythonMCPServer::safePath(const QString &rawPath, QString *error) const
{
    QString candidate = rawPath;
    if (!QFileInfo(candidate).isAbsolute())
        candidate = QDir::cleanPath(m_workDir + "/" + candidate);
    else
        candidate = QDir::cleanPath(candidate);

    if (!candidate.startsWith(m_workDir)) {
        if (error) *error = "Access denied: path is outside the working directory.";
        return {};
    }
    return candidate;
}

QString PythonMCPServer::envPath(const QString &envName) const
{
    return QDir::cleanPath(m_envsDir + "/" + envName);
}

QString PythonMCPServer::pythonExecutable(const QString &envName) const
{
    if (!envName.isEmpty()) {
        const QString base = envPath(envName);

#ifdef Q_OS_WIN
        const QString candidate = base + "/Scripts/python.exe";
#else
        const QString candidate = base + "/bin/python";
#endif
        if (QFile::exists(candidate))
            return candidate;
        return {};
    }

    for (const QString c : { "python3", "python" }) {
        QProcess p;
        p.start(c, { "--version" });
        if (p.waitForFinished(3000) && p.exitCode() == 0)
            return c;
    }
    return {};
}

QString PythonMCPServer::pipExecutable(const QString &envName) const
{
    if (!envName.isEmpty()) {
        const QString base = envPath(envName);

#ifdef Q_OS_WIN
        const QString candidate = base + "/Scripts/pip.exe";
#else
        const QString candidate = base + "/bin/pip";
#endif
        if (QFile::exists(candidate))
            return candidate;

        const QString python = pythonExecutable(envName);
        return python.isEmpty() ? QString() : python + " -m pip (use runProcess override)";
    }

    for (const QString c : { "pip3", "pip" }) {
        QProcess p;
        p.start(c, { "--version" });
        if (p.waitForFinished(3000) && p.exitCode() == 0)
            return c;
    }
    return {};
}

MCPToolResult PythonMCPServer::runProcess(const QString &program,
                                          const QStringList &arguments,
                                          int timeoutMs,
                                          const QString &workingDir) const
{
    QString realProgram = program;
    QStringList realArgs = arguments;
    if (program.endsWith(" -m pip (use runProcess override)")) {
        realProgram = program.section(' ', 0, 0);
        realArgs.prepend("pip");
        realArgs.prepend("-m");
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    if (!workingDir.isEmpty())
        proc.setWorkingDirectory(workingDir);

    proc.start(realProgram, realArgs);

    if (!proc.waitForStarted(5000))
        return MCPToolResult::error(
            QString("Failed to start process '%1': %2")
            .arg(realProgram, proc.errorString()));

    const bool finished = proc.waitForFinished(timeoutMs);
    const QString output = QString::fromUtf8(proc.readAll());

    if (!finished) {
        proc.kill();
        return MCPToolResult::error(
            QString("Process timed out after %1 s.\n\nPartial output:\n%2")
            .arg(timeoutMs / 1000)
            .arg(output));
    }

    const int exitCode = proc.exitCode();
    const QString header = exitCode == 0
        ? QString("Exit code: 0\n\n")
        : QString("Exit code: %1\n\n").arg(exitCode);

    MCPToolResult result;
    result.isError = (exitCode != 0);
    result.text    = header + (output.isEmpty() ? "(no output)" : output);
    return result;
}

bool PythonMCPServer::isSafeFileName(const QString &name)
{
    if (name.isEmpty() || name == "." || name == "..")
        return false;

    if (name.contains('/') || name.contains('\\') || name.contains(".."))
        return false;

    static const QRegularExpression re(R"(^[\w\-\.]+$)");
    return re.match(name).hasMatch();
}
