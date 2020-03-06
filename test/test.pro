TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        src/feccodectest.cpp \
    src/testmain.cpp \
    src/wirehairtest.cpp

win32{
LIBS += -L$$PWD/SDK/win32/lib/ -lgtest \
        -L$$PWD/SDK/win32/lib/ -lgmock \
        -L$$PWD/../../build-rtplivelib-Desktop_Qt_5_12_2_MinGW_64_bit-Debug/debug/ -lrtplivelib

INCLUDEPATH += $$PWD/SDK/win32/include
DEPENDPATH += $$PWD/SDK/win32/include
}


INCLUDEPATH += $$PWD/../src
DEPENDPATH += $$PWD/../src

HEADERS += \
    src/fectest.h

