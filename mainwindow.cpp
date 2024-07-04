#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QUdpSocket>
#include <XInput.h>
#include <QThread>
#include <iostream>
#include <algorithm>

using namespace std;

int fthruster1 = 1500;
int fthruster2 = 1500;
int lthruster = 1500;
int rthruster = 1500;
int dthruster1 = 1500;
int dthruster2 = 1500;

QString ESP_IP="192.168.1.100";

int phdata;
int tempdata;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    cap(),
    videoTimer(new QTimer(this)),
    controllerTimer(new QTimer(this)),
    udpTimer(new QTimer(this)),
    udpSocket(new QUdpSocket(this))
{
    ui->setupUi(this);

    label1 = ui->label_1;
    label2 = ui->label_2;
    controllerStatusLabel = ui->label_controller_status;

    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::startCamera);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::stopCamera);

    connect(videoTimer, SIGNAL(timeout()), this, SLOT(captureFrame()));
    connect(controllerTimer, SIGNAL(timeout()), this, SLOT(readXboxController()));
    controllerTimer->start(100);

    if (!udpSocket->bind(QHostAddress::Any, 10010)) {
        qDebug() << "Error binding UDP socket:" << udpSocket->errorString();
    } else {
        qDebug() << "UDP socket bound to port" << udpSocket->localPort();
    }

    udpTimer->start(100);

    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readESP()));
}

MainWindow::~MainWindow()
{
    stopCamera();
    delete ui;
}

void MainWindow::startCamera()
{
    if (cap.isOpened()) {
        qDebug() << "Camera already started";
        return;
    }

    cap.open("rtsp://admin:admin@192.168.1.10");

    if (!cap.isOpened()) {
        qDebug() << "Error: Could not open RTSP stream";
        return;
    }

    videoTimer->start(30);
}

void MainWindow::stopCamera()
{
    if (!cap.isOpened()) {
        qDebug() << "Camera already stopped";
        return;
    }

    videoTimer->stop();
    cap.release();

    label1->clear();
    label2->clear();
}

void MainWindow::captureFrame()
{
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        qDebug() << "Error: Empty frame";
        return;
    }

    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
    QImage qimg(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);

    label1->setPixmap(QPixmap::fromImage(qimg));
    label2->setPixmap(QPixmap::fromImage(qimg));
}

void MainWindow::readXboxController()
{
    DWORD result = XInputGetState(0, &controllerState);
    if (result == ERROR_SUCCESS) {
        processControllerInput();
    } else {
        controllerStatusLabel->setText("Xbox controller not found or not connected");
    }
}

// Function to map controller input to thruster value
int mapControllerToThruster(int controllerValue) {
    // Controller range
    const int controllerMin = -34268;
    const int controllerMax = 34018;

    // Thruster range
    const int thrusterMin = 1200;
    const int thrusterMax = 1800;
    const int thrusterNeutral = 1500;

    // Map the controller value to the range [-1, 1]
    double normalizedValue = (static_cast<double>(controllerValue) - controllerMin) / (controllerMax - controllerMin) * 2 - 1;

    // Map the normalized value to the thruster range
    int thrusterValue = static_cast<int>((normalizedValue + 1) / 2 * (thrusterMax - thrusterMin) + thrusterMin);

    // Clamp the thruster value to its min and max
    thrusterValue = max(thrusterMin, min(thrusterValue, thrusterMax));

    return thrusterValue;
}


void MainWindow::processControllerInput()
{
    XINPUT_GAMEPAD* pad = &controllerState.Gamepad;
    buttonStatus.clear();

    if (pad->wButtons & XINPUT_GAMEPAD_A) {
        buttonStatus << "Button A pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_B) {
        buttonStatus << "Button B pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_X) {
        buttonStatus << "Button X pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_Y) {
        buttonStatus << "Button Y pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) {
        buttonStatus << "Left Shoulder pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
        buttonStatus << "Right Shoulder pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_BACK) {
        buttonStatus << "Back button pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_START) {
        buttonStatus << "Start button pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB) {
        buttonStatus << "Left Thumbstick pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) {
        buttonStatus << "Right Thumbstick pressed";
    }

    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        buttonStatus << "DPad Up pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        buttonStatus << "DPad Down pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        buttonStatus << "DPad Left pressed";
    }
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        buttonStatus << "DPad Right pressed";
    }

    if (pad->bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        buttonStatus << "Left Trigger pressed";
    }
    if (pad->bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        buttonStatus << "Right Trigger pressed";
    }

    // bool thumbstickMoved = false;

    if (pad->sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || pad->sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        fthruster1 = mapControllerToThruster(pad->sThumbLX);
        fthruster2 = mapControllerToThruster(pad->sThumbLX);
        sendThrusterData();
    }
    if (pad->sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || pad->sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        dthruster1 = mapControllerToThruster(pad->sThumbLY);
        dthruster2 = mapControllerToThruster(pad->sThumbLY);
        sendThrusterData();
    }
    if (pad->sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || pad->sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
        rthruster = mapControllerToThruster(pad->sThumbRX);
        lthruster = mapControllerToThruster(pad->sThumbRX);
        sendThrusterData();
    }
    // if (pad->sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || pad->sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
    //     sendThrusterData();
    // }

    // if (pad->wButtons & XINPUT_GAMEPAD_A) {
    //     resetThrusterValues();
    //     sendThrusterData();
    // }

    else{
        resetThrusterValues();
        sendThrusterData();
        }
}

void MainWindow::resetThrusterValues()
{
    fthruster1 = 1500;
    fthruster2 = 1500;
    lthruster = 1500;
    rthruster = 1500;
    dthruster1 = 1500;
    dthruster2 = 1500;
}

void MainWindow::sendThrusterData()
{
    QString data = QString("%1/%2/%3/%4/%5/%6")
                       .arg(fthruster1)
                       .arg(fthruster2)
                       .arg(lthruster)
                       .arg(rthruster)
                       .arg(dthruster1)
                       .arg(dthruster2);
    qDebug() << data;
    QByteArray byteArray = data.toUtf8();
    udpSocket->writeDatagram(byteArray, QHostAddress(ESP_IP), 10011);
}

void MainWindow::readESP()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(udpSocket->pendingDatagramSize()));
        udpSocket->readDatagram(datagram.data(), datagram.size());

        QString message = QString::fromUtf8(datagram);

        QMetaObject::invokeMethod(this, [this, message]() {
                controllerStatusLabel->setText(message);
            }, Qt::QueuedConnection);
    }
}
