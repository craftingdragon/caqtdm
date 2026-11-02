include(../tst_plugins.pri)
QT += positioning
SOURCES += tst_gps.cpp ../../../caQtDM_Plugins/gps/gps_plugin.cpp
HEADERS += ../../../caQtDM_Plugins/gps/gps_plugin.h
INCLUDEPATH += ../../../caQtDM_Lib/src ../../../caQtDM_Plugins ../../../caQtDM_Plugins/gps
LIBS += -L$$(CAQTDM_COLLECT) -lqtcontrols -lcaQtDM_Lib
CONFIG += caqtdm_rpath
include(../../../caQtDM.pri)
