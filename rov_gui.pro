QT += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

INCLUDEPATH += C:\opencv\build\include
INCLUDEPATH += C:\serial\include
INCLUDEPATH += C:\serial\build

LIBS+= -lws2_32
LIBS += -lxinput
LIBS += C:\opencv-build\bin\libopencv_core490.dll
LIBS += C:\opencv-build\bin\libopencv_highgui490.dll
LIBS += C:\opencv-build\bin\libopencv_imgcodecs490.dll
LIBS += C:\opencv-build\bin\libopencv_imgproc490.dll
LIBS += C:\opencv-build\bin\libopencv_features2d490.dll
LIBS += C:\opencv-build\bin\libopencv_calib3d490.dll
LIBS += C:\opencv-build\bin\libopencv_videoio490.dll

# more correct variant, how set includepath and libs for mingw
# add system variable: OPENCV_SDK_DIR=D:/opencv/opencv-build/install
# read http://doc.qt.io/qt-5/qmake-variable-reference.html#libs

#INCLUDEPATH += $$(OPENCV_SDK_DIR)/include

#LIBS += -L$$(OPENCV_SDK_DIR)/x86/mingw/lib \
#        -lopencv_core320        \
#        -lopencv_highgui320     \
#        -lopencv_imgcodecs320   \
#        -lopencv_imgproc320     \
#        -lopencv_features2d320  \
#        -lopencv_calib3d320

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
