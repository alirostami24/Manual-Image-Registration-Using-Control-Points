#-------------------------------------------------
#
# Project created by QtCreator 2019-06-01T12:44:13
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

RC_ICONS += icon.ico

TARGET = ManualRegistrationUI
TEMPLATE = app


SOURCES += \
    Dialog.cpp \
    Mainui.cpp \
    Main.cpp

HEADERS  += \
    Dialog.h \
    Mainui.h

FORMS    += \
    Dialog.ui \
    Mainui.ui

win32:VERSION = 1.3.2.1032 # major.minor.patch.build
else:VERSION = 1.3.2    # major.minor.patch

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Release/' -lopencv_core310
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Debug/' -lopencv_core310d
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Release/' -lopencv_highgui310
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Debug/' -lopencv_highgui310d
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Release/' -lopencv_imgproc310
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Debug/' -lopencv_imgproc310d
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Release/' -lopencv_photo310
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Debug/' -lopencv_photo310d
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Release/' -lopencv_imgcodecs310
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Debug/' -lopencv_imgcodecs310d
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Release/' -lopencv_videoio310
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Debug/' -lopencv_videoio310d
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Release/' -lopencv_video310
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/'..HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Debug/' -lopencv_video310d
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Release/' -lopencv_calib3d310
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/lib/Debug/' -lopencv_calib3d310d

INCLUDEPATH += $$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/include'
DEPENDPATH += $$PWD/'../HyperGIS-widget/OpenCV 3.1 x86- complete/include'


