TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

XQLIB += network io
include(/home/felix/projects/xqlib/xqlib.pri)

PACKAGES = sdl2 glm assimp

QMAKE_CFLAGS += $$system(pkg-config --cflags $$PACKAGES)
QMAKE_CXXFLAGS += $$system(pkg-config --cflags $$PACKAGES)
QMAKE_LFLAGS += $$system(pkg-config --libs $$PACKAGES)

SOURCES += \
        main.cpp \
        mesh.cpp

HEADERS += \
  mesh.hpp
