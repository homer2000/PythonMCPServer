#include "QMCPServer.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QDateTime>
#include <QFileInfo>
#include <cstdio>

#ifdef Q_OS_WIN
#  include <io.h>
#  include <fcntl.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Traffic log implementation
// ─────────────────────────────────────────────────────────────────────────────

void QMCPServer::trafficLog(const char *direction, const QByteArray &data)
{
    if constexpr (!kEnableMCPTrafficLog) return;

    //QMutexLocker lock(&m_logMutex);
    m_logMutex.lock();
    if (!m_logFile.isOpen()) return;

    // Формат: [2026-05-11 13:10:30.123] >>> {...}
    const QString ts = QDateTime::currentDateTime()
                           .toString("yyyy-MM-dd HH:mm:ss.zzz");
    QByteArray line;
    line += '[';
    line += ts.toUtf8();
    line += "] ";
    line += direction;
    line += ' ';
    line += data.trimmed();
    line += '\n';

    m_logFile.write(line);
    m_logFile.flush();
    m_logMutex.unlock();
}

void QMCPServer::debugLog(QString data) {
    m_debugLogMutex.lock();
    if (m_debugLogFile.isOpen()) {
        const QString ts = data + "   " + QDateTime::currentDateTime()
                                    .toString("yyyy-MM-dd HH:mm:ss.zzz") + "\n";
        m_debugLogFile.write(ts.toUtf8());
        m_debugLogFile.flush();

    }
    m_debugLogMutex.unlock();
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

QMCPServer::QMCPServer(const QString &serverName,
                       const QString &serverVersion,
                       QObject *parent)
    : QObject(parent)
    , m_serverName(serverName)
    , m_serverVersion(serverVersion)
{}

QMCPServer::~QMCPServer()
{
    if (m_reader) {
        m_reader->requestInterruption();
        m_reader->deleteLater();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void QMCPServer::start()
{
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    setvbuf(stdout, nullptr, _IONBF, 0);

    // ── Traffic log ───────────────────────────────────────────────────────────
    if constexpr (kEnableMCPTrafficLog) {
        // Лог кладём рядом с бинарём: <dir>/mcp_traffic.log
        const QString binDir = QFileInfo(QCoreApplication::applicationFilePath())
                                   .absolutePath();
        m_logFile.setFileName(binDir + "/mcp_traffic.log");
        if (m_logFile.open(QIODevice::Append | QIODevice::Text)) {
            //QMutexLocker lock(&m_logMutex);
            m_logMutex.lock();
            const QString header = QString("\n=== SESSION START %1 pid=%2 ===\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                .arg(QCoreApplication::applicationPid());
            m_logFile.write(header.toUtf8());
            m_logFile.flush();
            m_logMutex.unlock();
        } else {
            dbg(QString("[QMCPServer] WARNING: cannot open traffic log: %1")
                .arg(m_logFile.errorString()));
        }

        // ── Traffic log ───────────────────────────────────────────────────────────
        // Лог кладём рядом с бинарём: <dir>/mcp_traffic.log
        const QString binDebugDir = QFileInfo(QCoreApplication::applicationFilePath())
                                   .absolutePath();
        m_debugLogFile.setFileName(binDebugDir + "/debug.log");
        if (m_debugLogFile.open(QIODevice::Append | QIODevice::Text)) {
            m_debugLogMutex.lock();

            const QString header = QString("\nStart %1 pid=%2\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                .arg(QCoreApplication::applicationPid());
            m_debugLogFile.write(header.toUtf8());
            m_debugLogFile.flush();

            m_debugLogMutex.unlock();
        }

    }

    dbg(QString("[QMCPServer] %1 v%2 starting").arg(m_serverName, m_serverVersion));

    m_reader = new StdinReaderThread(this);
    connect(m_reader, &StdinReaderThread::lineReceived,
            this,     &QMCPServer::onLineReceived,
            Qt::DirectConnection);
    connect(m_reader, &StdinReaderThread::eofReached,
            this,     &QMCPServer::onEof,
            Qt::DirectConnection);
    m_reader->start();
}

void QMCPServer::registerTool(const MCPToolDefinition &def)
{
    m_tools.insert(def.name, def);
}

// ─────────────────────────────────────────────────────────────────────────────
// sendToolResult — вызывается подклассом из любого потока после async-работы
// ─────────────────────────────────────────────────────────────────────────────

void QMCPServer::sendToolResult(const QJsonValue &id, const MCPToolResult &res)
{
    QJsonArray content;
    if (!res.text.isEmpty())
        content.append(QJsonObject { {"type","text"}, {"text", res.text} });

    QJsonObject result {
        { "content", content },
        { "isError", res.isError }
    };
    if (!res.structuredContent.isEmpty())
        result.insert("structuredContent", res.structuredContent);

    // writeMessage защищён мьютексом — безопасен из любого потока
    sendResult(id, result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots (main thread)
// ─────────────────────────────────────────────────────────────────────────────

void QMCPServer::onLineReceived(const QByteArray &line)
{
    trafficLog(">>>", line);
    dbg(QString("[QMCPServer] <<< %1").arg(QString::fromUtf8(line)));

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        sendError(QJsonValue::Null, -32700,
                  "Parse error: " + err.errorString());
        return;
    }

    dispatch(doc.object());
}

void QMCPServer::onEof()
{
    dbg("[QMCPServer] stdin closed");
    QCoreApplication::quit();
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON-RPC dispatch
// ─────────────────────────────────────────────────────────────────────────────

void QMCPServer::dispatch(const QJsonObject &msg)
{
    const QString    method    = msg.value("method").toString();
    const QJsonValue id        = msg.value("id");
    const bool       hasId     = msg.contains("id");
    const bool       hasMethod = msg.contains("method");

    if (hasMethod && !hasId) {
        if (method == "notifications/initialized") {
            m_state = State::Ready;
            dbg("[QMCPServer] handshake complete — Ready");
            emit clientInitialized();
            onInitialized();
        }
        return;
    }

    if (hasMethod && hasId) {
        if (method == "initialize") {
            handleInitialize(msg);
        } else if (method == "ping") {
            handlePing(msg);
        } else if (method == "tools/list") {
            if (m_state == State::Idle) { sendError(id, -32002, "Not initialized"); return; }
            handleToolsList(msg);
        } else if (method == "tools/call") {
            if (m_state == State::Idle) { sendError(id, -32002, "Not initialized"); return; }
            handleToolsCall(msg);
        } else {
            sendError(id, -32601, "Method not found: " + method);
        }
        return;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MCP handlers
// ─────────────────────────────────────────────────────────────────────────────

void QMCPServer::handleInitialize(const QJsonObject &msg)
{
    const QJsonValue  id     = msg.value("id");
    const QJsonObject params = msg.value("params").toObject();

    const QJsonObject clientInfo = params.value("clientInfo").toObject();
    dbg(QString("[QMCPServer] client: %1 %2")
        .arg(clientInfo.value("name").toString(),
             clientInfo.value("version").toString()));

    m_state = State::Initializing;

    QJsonObject result {
        { "protocolVersion", kProtocolVersion },
        { "capabilities",    buildCapabilities() },
        { "serverInfo", QJsonObject {
            { "name",    m_serverName    },
            { "version", m_serverVersion }
        }}
    };
    if (!m_instructions.isEmpty())
        result.insert("instructions", m_instructions);

    sendResult(id, result);
}

void QMCPServer::handleToolsList(const QJsonObject &msg)
{
    QJsonArray toolsArr;
    for (const MCPToolDefinition &def : qAsConst(m_tools)) {
        toolsArr.append(QJsonObject {
            { "name",        def.name },
            { "description", def.description },
            { "inputSchema", buildInputSchema(def.params) }
        });
    }
    sendResult(msg.value("id"), QJsonObject { { "tools", toolsArr } });
}

void QMCPServer::handleToolsCall(const QJsonObject &msg)
{
    const QJsonValue  id     = msg.value("id");
    const QJsonObject params = msg.value("params").toObject();
    const QString     name   = params.value("name").toString();
    const QJsonObject args   = params.value("arguments").toObject();

    if (!m_tools.contains(name)) {
        sendResult(id, QJsonObject {
            { "content", QJsonArray {
                QJsonObject { {"type","text"}, {"text","Unknown tool: "+name} }
            }},
            { "isError", true }
        });
        return;
    }

    emit toolCallReceived(name, args);
    dbg(QString("[QMCPServer] → %1").arg(name));

    // ── Сначала пробуем асинхронный путь ─────────────────────────────────────
    // Если подкласс взял управление на себя — он вызовет sendToolResult(id, ...)
    // когда работа будет закончена. Мы здесь ничего не отправляем.
    if (handleToolCallAsync(id, name, args))
        return;

    // ── Синхронный путь (инструменты которые возвращают результат мгновенно) ──
    MCPToolResult res = handleToolCall(name, args);
    sendToolResult(id, res);
}

void QMCPServer::handlePing(const QJsonObject &msg)
{
    sendResult(msg.value("id"), QJsonObject {});
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility (protected)
// ─────────────────────────────────────────────────────────────────────────────

void QMCPServer::sendLog(const QString &message, const QString &level)
{
    sendNotification("notifications/message", QJsonObject {
        { "level",  level },
        { "logger", m_serverName },
        { "data",   message }
    });
}

void QMCPServer::notifyToolListChanged()
{
    sendNotification("notifications/tools/list_changed");
}

void QMCPServer::dbg(const QString &msg) const
{
    if (!m_debug) return;
    QByteArray line = msg.toUtf8();
    line.append('\n');
    fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stderr);
    fflush(stderr);
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON-RPC transport (private)
// ─────────────────────────────────────────────────────────────────────────────

void QMCPServer::sendResult(const QJsonValue &id, const QJsonValue &result)
{
    writeMessage(QJsonObject {
        { "jsonrpc", "2.0" },
        { "id",      id     },
        { "result",  result }
    });
}

void QMCPServer::sendError(const QJsonValue &id, int code, const QString &message)
{
    writeMessage(QJsonObject {
        { "jsonrpc", "2.0" },
        { "id", id },
        { "error", QJsonObject { {"code",code}, {"message",message} } }
    });
}

void QMCPServer::sendNotification(const QString &method, const QJsonObject &params)
{
    QJsonObject msg { {"jsonrpc","2.0"}, {"method",method} };
    if (!params.isEmpty()) msg.insert("params", params);
    writeMessage(msg);
}

void QMCPServer::writeMessage(const QJsonObject &obj)
{
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    data.append('\n');

    QMutexLocker lock(&m_writeMutex);
    fwrite(data.constData(), 1, static_cast<size_t>(data.size()), stdout);
    fflush(stdout);

    trafficLog("<<<", data);
    dbg(QString("[QMCPServer] >>> %1").arg(QString::fromUtf8(data).trimmed()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Schema / capability builders
// ─────────────────────────────────────────────────────────────────────────────

QJsonObject QMCPServer::buildInputSchema(const QList<MCPToolParam> &params) const
{
    QJsonObject properties;
    QJsonArray  required;

    for (const MCPToolParam &p : params) {
        properties.insert(p.name, QJsonObject {
            { "type",        p.type        },
            { "description", p.description }
        });
        if (p.required) required.append(p.name);
    }

    QJsonObject schema {
        { "type",       "object"   },
        { "properties", properties }
    };
    if (!required.isEmpty())
        schema.insert("required", required);
    if (params.isEmpty())
        schema.insert("additionalProperties", false);

    return schema;
}

QJsonObject QMCPServer::buildCapabilities() const
{
    return QJsonObject {
        { "tools",   QJsonObject { { "listChanged", true } } },
        { "logging", QJsonObject {} }
    };
}
