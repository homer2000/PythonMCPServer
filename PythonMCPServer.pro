QT       += core
QT       -= gui

TARGET    = PythonMCPServer
TEMPLATE  = app
CONFIG   += console c++17
CONFIG   -= app_bundle

# ── Sources ──────────────────────────────────────────────────────────────────

HEADERS += \
    src/QMCPServer.h \
    src/PythonMCPServer.h

SOURCES += \
    src/QMCPServer.cpp \
    src/main.cpp \
    src/PythonMCPServer.cpp

# ── Platform-specific ────────────────────────────────────────────────────────

win32 {
    # Требуется для _setmode / _fileno (бинарный режим stdin/stdout)
    LIBS += -lAdvapi32
}

unix:!macx {
    # Linux: явная линковка с pthread нужна некоторым версиям GCC
    LIBS += -lpthread
}

# ── Output directories ───────────────────────────────────────────────────────

DESTDIR     = $$PWD/build
OBJECTS_DIR = $$PWD/build
MOC_DIR     = $$PWD/build
RCC_DIR     = $$PWD/build