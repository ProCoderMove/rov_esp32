#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <XInput.h>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    cap(),
    videoTimer(new QTimer(this)),
    controllerTimer(new QTimer(this))
{
    ui->setupUi(this);

    label1 = ui->label_1;
    label2 = ui->label_2;
    controllerStatusLabel = ui->label_controller_status;

    // Connect buttons to slots
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::startCamera);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::stopCamera);

    // Set up video timer
    connect(videoTimer, SIGNAL(timeout()), this, SLOT(captureFrame()));

    // Set up controller input timer
    connect(controllerTimer, SIGNAL(timeout()), this, SLOT(readXboxController()));

    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        qDebug() << "WSAStartup failed with error:" << result;
        return;
    }

    // Create UDP socket
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        qDebug() << "Socket creation failed with error:" << WSAGetLastError();
        WSACleanup();
        return;
    }

    // Set up UDP address
    udpAddress.sin_family = AF_INET;
    udpAddress.sin_port = htons(12345); // Change to the appropriate port
    udpAddress.sin_addr.s_addr = inet_addr("127.0.0.1"); // Change to the appropriate address

    controllerTimer->start(30);
}

MainWindow::~MainWindow()
{
    stopCamera();
    closesocket(udpSocket);
    WSACleanup();
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

    // Start the video timer
    videoTimer->start(30);
}

void MainWindow::stopCamera()
{
    if (!cap.isOpened()) {
        qDebug() << "Camera already stopped";
        return;
    }

    // Stop the video timer
    videoTimer->stop();
    cap.release();
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
    DWORD result = XInputGetState(0, &controllerState); // Assuming controller 0
    if (result == ERROR_SUCCESS) {
        processControllerInput();
        controllerStatusLabel->setText("Xbox controller FOUND");
    } else {
        controllerStatusLabel->setText("Xbox controller not found or not connected");
    }
}

void MainWindow::processControllerInput()
{
    XINPUT_GAMEPAD* pad = &controllerState.Gamepad;
    QStringList buttonStatus;

    // Check each button individually
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

    // Check D-pad directions
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

    // Check left and right triggers
    if (pad->bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        buttonStatus << "Left Trigger pressed";
    }
    if (pad->bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        buttonStatus << "Right Trigger pressed";
    }

    // Check thumbstick positions
    if (pad->sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || pad->sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        buttonStatus << QString("Left Thumbstick X: %1").arg(pad->sThumbLX);
    }
    if (pad->sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || pad->sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        buttonStatus << QString("Left Thumbstick Y: %1").arg(pad->sThumbLY);
    }
    if (pad->sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || pad->sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
        buttonStatus << QString("Right Thumbstick X: %1").arg(pad->sThumbRX);
    }
    if (pad->sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || pad->sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
        buttonStatus << QString("Right Thumbstick Y: %1").arg(pad->sThumbRY);
    }

    // Join the button status list into a single string
    QString statusMessage = buttonStatus.join("; ");

    // Send the button status over UDP
    QByteArray data = statusMessage.toUtf8();
    int result = sendto(udpSocket, data.data(), data.size(), 0, (sockaddr*)&udpAddress, sizeof(udpAddress));
    if (result == SOCKET_ERROR) {
        qDebug() << "sendto failed with error:" << WSAGetLastError();
    }

    // Update the controller status label with the current button presses
    controllerStatusLabel->setText(buttonStatus.join("\n"));
    qDebug() << buttonStatus;
}
