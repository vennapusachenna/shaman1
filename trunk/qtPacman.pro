######################################################################
# Automatically generated by qmake (2.01a) gio gen 31 17:54:18 2008
######################################################################

CONFIG += qt debug
TEMPLATE = app
TARGET = 
DEPENDPATH += . src ui
INCLUDEPATH += . src
LIBS += -lalpm

# Input
HEADERS += src/AlpmHandler.h \
           src/callbacks.h \
           src/ConfigurationParser.h \
           src/MainWindow.h \
           src/StringUtils.h \
           src/UpdateDbDialog.h \
           src/SysUpgradeDialog.h \
           src/QueueDialog.h
FORMS += ui/MainWindow.ui \
	 ui/dbUpdateDialog.ui \
	 ui/upgradeDialog.ui \
	 ui/transactionDialog.ui
SOURCES += src/AlpmHandler.cpp \
           src/callbacks.cpp \
           src/ConfigurationParser.cpp \
           src/main.cpp \
           src/MainWindow.cpp \
           src/StringUtils.cpp \
           src/UpdateDbDialog.cpp \
           src/SysUpgradeDialog.cpp \
           src/QueueDialog.cpp
