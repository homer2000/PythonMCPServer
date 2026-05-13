#pragma once

#include "QMCPServer.h"

#include <QObject>
#include <QMap>
#include <QDir>
#include <QProcess>

// ─────────────────────────────────────────────────────────────────────────────
// PythonMCPServer
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief MCP-сервер для создания, управления и запуска Python-скриптов.
 *
 * Предоставляет следующие инструменты (tools):
 *
 *  === Управление скриптами ===
 *  python_create_script        — создать (или перезаписать) .py-файл
 *  python_run_script           — запустить скрипт (из глобального или venv Python)
 *  python_list_scripts         — перечислить скрипты в рабочей директории
 *  python_read_script          — прочитать содержимое скрипта
 *  python_delete_script        — удалить скрипт
 *
 *  === Управление окружениями ===
 *  python_create_env           — создать виртуальное окружение (venv)
 *  python_delete_env           — удалить виртуальное окружение
 *  python_activate_env         — активировать окружение для последующих команд
 *  python_list_envs            — список созданных venv-окружений
 *  python_list_packages        — список установленных пакетов в окружении
 *
 *  === Управление пакетами ===
 *  python_install_package      — установить пакет(ы) через pip
 *  python_install_requirements — установить зависимости из requirements.txt
 *  python_create_requirements  — создать requirements.txt из окружения
 *  python_export_env           — экспортировать окружение как конфигурацию
 *  python_import_env           — импортировать окружение из конфигурации
 *
 *  === Выполнение и отладка ===
 *  python_execute              — выполнить Python-код напрямую (REPL)
 *  python_debug_script         — запустить скрипт в режиме отладки
 *  python_profile_script       — профилирование выполнения
 *
 *  === Работа с файлами ===
 *  python_read_file            — прочитать произвольный файл как текст
 *  python_write_file           — записать текст/код в произвольный файл
 *  python_find_imports         — найти импорты в скрипте
 *
 *  === Мониторинг ===
 *  python_memory_usage         — показать использование памяти скриптом
 *  python_process_info         — информация о запущенном процессе Python
 *
 * @code
 * int main(int argc, char *argv[])
 * {
 *     QCoreApplication app(argc, argv);
 *     PythonMCPServer server;
 *     server.setDebug(true);
 *     server.start();
 *     return app.exec();
 * }
 * @endcode
 */
class PythonMCPServer : public QMCPServer
{
    Q_OBJECT

public:
    explicit PythonMCPServer(QObject *parent = nullptr);

    /**
     * @brief Установить рабочую директорию для скриптов и окружений.
     *        Должна вызываться ДО start().
     */
    void setWorkDir(const QString &path);

    /** Вернуть текущую рабочую директорию. */
    QString workDir() const { return m_workDir; }

protected:
    MCPToolResult handleToolCall(const QString &name,
                                 const QJsonObject &args) override;

private:
    // ── Управление скриптами ─────────────────────────────────────────────────

    MCPToolResult toolCreateScript(const QJsonObject &args);
    MCPToolResult toolRunScript(const QJsonObject &args);
    MCPToolResult toolListScripts(const QJsonObject &args);
    MCPToolResult toolReadScript(const QJsonObject &args);
    MCPToolResult toolDeleteScript(const QJsonObject &args);

    // ── Управление окружениями ───────────────────────────────────────────────

    MCPToolResult toolCreateEnv(const QJsonObject &args);
    MCPToolResult toolDeleteEnv(const QJsonObject &args);
    MCPToolResult toolActivateEnv(const QJsonObject &args);
    MCPToolResult toolListEnvs(const QJsonObject &args);
    MCPToolResult toolListPackages(const QJsonObject &args);

    // ── Управление пакетами ──────────────────────────────────────────────────

    MCPToolResult toolInstallPackage(const QJsonObject &args);
    MCPToolResult toolInstallRequirements(const QJsonObject &args);
    MCPToolResult toolCreateRequirements(const QJsonObject &args);
    MCPToolResult toolExportEnv(const QJsonObject &args);
    MCPToolResult toolImportEnv(const QJsonObject &args);

    // ── Выполнение и отладка ─────────────────────────────────────────────────

    MCPToolResult toolExecute(const QJsonObject &args);
    MCPToolResult toolDebugScript(const QJsonObject &args);
    MCPToolResult toolProfileScript(const QJsonObject &args);

    // ── Работа с файлами ─────────────────────────────────────────────────────

    MCPToolResult toolReadFile(const QJsonObject &args);
    MCPToolResult toolWriteFile(const QJsonObject &args);
    MCPToolResult toolFindImports(const QJsonObject &args);

    // ── Мониторинг ───────────────────────────────────────────────────────────

    MCPToolResult toolMemoryUsage(const QJsonObject &args);
    MCPToolResult toolProcessInfo(const QJsonObject &args);

    // ── Вспомогательные методы ───────────────────────────────────────────────

    /** Полный путь к скрипту (валидирует имя, не допускает выход за workDir). */
    QString scriptPath(const QString &name, QString *error = nullptr) const;

    /** Путь к директории venv. */
    QString envPath(const QString &envName) const;

    /**
     * @brief Найти исполняемый Python внутри venv или в системе.
     * @param envName  Имя окружения (пустая строка → активное или системный Python).
     */
    QString pythonExecutable(const QString &envName) const;

    /**
     * @brief Найти pip внутри venv или в системе.
     */
    QString pipExecutable(const QString &envName) const;

    /**
     * @brief Запустить процесс синхронно с таймаутом.
     * @param program    Исполняемый файл.
     * @param arguments  Аргументы.
     * @param timeoutMs  Таймаут в миллисекундах (−1 = без ограничений).
     * @param workingDir Рабочая директория процесса.
     * @return MCPToolResult с stdout/stderr и кодом возврата.
     */
    MCPToolResult runProcess(const QString &program,
                             const QStringList &arguments,
                             int timeoutMs = 30000,
                             const QString &workingDir = {}) const;

    /** Проверить, является ли имя файла безопасным (нет /, .., ...). */
    static bool isSafeFileName(const QString &name);

    /** Нормализовать путь к файлу, проверив выход за workDir. */
    QString safePath(const QString &rawPath, QString *error = nullptr) const;

    // ────────────────────────────────────────────────────────────────────────

    QString m_workDir;
    QString m_envsDir;          // m_workDir + "/envs"
    QString m_activeEnv;        // имя активного окружения (пусто = системный Python)

    static constexpr int kDefaultTimeoutMs = 60'000;   // 60 с для pip install
    static constexpr int kRunTimeoutMs     = 30'000;   // 30 с для скрипта
    static constexpr int kProfileTimeoutMs = 120'000;  // 120 с для профилирования
};
