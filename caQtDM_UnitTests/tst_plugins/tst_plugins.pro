include(../../caQtDM_Viewer/qtdefs.pri)

TEMPLATE = subdirs

SUBDIRS += tst_internal

gps: SUBDIRS += tst_gps

bsread: {
    SUBDIRS += tst_bsread
}
