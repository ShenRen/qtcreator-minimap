include(../../../shared/trk/trk.pri)

HEADERS += \
    $$PWD/abstractgdbadapter.h \
    $$PWD/plaingdbadapter.h \
    $$PWD/gdbmi.h \
    $$PWD/gdbengine.h \
    $$PWD/gdboptionspage.h \
    $$PWD/remotegdbadapter.h \
    $$PWD/trkgdbadapter.h \
    $$PWD/trkoptions.h \
    $$PWD/trkoptionswidget.h \
    $$PWD/trkoptionspage.h

SOURCES += \
    $$PWD/gdbmi.cpp \
    $$PWD/gdbengine.cpp \
    $$PWD/gdboptionspage.cpp \
    $$PWD/plaingdbadapter.cpp \
    $$PWD/remotegdbadapter.cpp \
    $$PWD/trkoptions.cpp \
    $$PWD/trkoptionswidget.cpp \
    $$PWD/trkoptionspage.cpp \
    $$PWD/trkgdbadapter.cpp

FORMS +=  $$PWD/gdboptionspage.ui \
$$PWD/trkoptionswidget.ui

RESOURCES += $$PWD/gdb.qrc
