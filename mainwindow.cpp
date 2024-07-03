#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QUdpSocket>
#include <XInput.h>
#include <QThread>

int fthruster1 = 1500;
int fthruster2 = 1500;
int lthruster = 1500;
int rthruster = 1500;
int dthruster1 = 1500;
int dthruster2 = 1500;

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
    controllerTimer->start(170);

    if (!udpSocket->bind(QHostAddress::Any, 10010)) {
        qDebug() << "Error binding UDP socket:" << udpSocket->errorString();
    } else {
        qDebug() << "UDP socket bound to port" << udpSocket->localPort();
    }

    connect(udpTimer, SIGNAL(timeout()), this, SLOT(checkForUdpMessages()));
    udpTimer->start(170);

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

    bool thumbstickMoved = false;

    if (pad->sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || pad->sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        fthruster1 = 1500 + pad->sThumbLX;
        fthruster2 = 1500 - pad->sThumbLX;
        buttonStatus << QString("Left Thumbstick X: %1").arg(pad->sThumbLX);
        thumbstickMoved = true;
        sendThrusterData();
    }
    if (pad->sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || pad->sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        dthruster1 = 1500 + pad->sThumbLY;
        dthruster2 = 1500 - pad->sThumbLY;
        buttonStatus << QString("Left Thumbstick Y: %1").arg(pad->sThumbLY);
        thumbstickMoved = true;
        sendThrusterData();
    }
    if (pad->sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || pad->sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
        rthruster = 1500 + pad->sThumbRX;
        lthruster = 1500 - pad->sThumbRX;
        buttonStatus << QString("Right Thumbstick X: %1").arg(pad->sThumbRX);
        thumbstickMoved = true;
        sendThrusterData();
    }
    if (pad->sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || pad->sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
        buttonStatus << QString("Right Thumbstick Y: %1").arg(pad->sThumbRY);
        thumbstickMoved = true;
        sendThrusterData();
    }

    if (pad->wButtons & XINPUT_GAMEPAD_A) {
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
    udpSocket->writeDatagram(byteArray, QHostAddress("192.168.1.101"), 10011);
}

void MainWindow::readESP()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(udpSocket->pendingDatagramSize()));
        udpSocket->readDatagram(datagram.data(), datagram.size());
        // sendThrusterData();

        QString message = QString::fromUtf8(datagram);

        QMetaObject::invokeMethod(this, [this, message]() {
                controllerStatusLabel->setText(message);
            }, Qt::QueuedConnection);
    }
}
