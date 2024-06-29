#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <windows.h>
#include <XInput.h>
#include <QUdpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startCamera();
    void stopCamera();
    void captureFrame();
    void readXboxController();
    void processControllerInput();


private:
    Ui::MainWindow *ui;
    cv::VideoCapture cap;
    QTimer *videoTimer;
    QTimer *controllerTimer;
    QLabel *label1;
    QLabel *label2;
    QLabel *controllerStatusLabel;
    XINPUT_STATE controllerState;
    QUdpSocket *udpSocket; // UDP socket for sending data
    QStringList buttonStatus; // Store button status

};

#endif // MAINWINDOW_H
