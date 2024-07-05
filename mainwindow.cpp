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


using namespace cv;

Mat whiteBalance(const Mat& img) {
    Mat lab;
    cvtColor(img, lab, COLOR_BGR2Lab);

    vector<Mat> lab_channels;
    split(lab, lab_channels);

    double avg_a = mean(lab_channels[1])[0];
    double avg_b = mean(lab_channels[2])[0];

    lab_channels[1] = lab_channels[1] - ((avg_a - 128) * (lab_channels[0] / 255.0) * 1.1);
    lab_channels[2] = lab_channels[2] - ((avg_b - 128) * (lab_channels[0] / 255.0) * 1.1);

    merge(lab_channels, lab);
    Mat result;
    cvtColor(lab, result, COLOR_Lab2BGR);

    return result;
}

Mat histogramEqualization(const Mat& img) {
    Mat yuv;
    cvtColor(img, yuv, COLOR_BGR2YUV);

    vector<Mat> yuv_channels;
    split(yuv, yuv_channels);
    equalizeHist(yuv_channels[0], yuv_channels[0]);

    merge(yuv_channels, yuv);
    Mat result;
    cvtColor(yuv, result, COLOR_YUV2BGR);

    return result;
}
Mat dehaze(const Mat& img) {
    const float omega = 0.95f; // Added 'f' for consistency with float type
    const float t_min = 0.1f;  // Added 'f' for consistency with float type
    const int win_size = 15;

    // Convert to float for accurate division operations
    Mat img_float;
    img.convertTo(img_float, CV_32F, 1.0 / 255.0); // Convert to float and normalize to [0, 1]

    // Estimate the dark channel
    Mat dark_channel(img.size(), CV_32F); // Use CV_32F to match img_float type
    Mat min_img;
    erode(img_float, min_img, getStructuringElement(MORPH_RECT, Size(win_size, win_size))); // Use img_float instead of img
    vector<Mat> channels;
    split(min_img, channels);
    dark_channel = min(channels[0], min(channels[1], channels[2])); // Find the minimum across channels

    // Estimate the atmospheric light
    Mat dilated_img;
    dilate(img_float, dilated_img, getStructuringElement(MORPH_RECT, Size(win_size, win_size))); // Use img_float instead of img
    Scalar A = mean(dilated_img); // Calculate mean to get atmospheric light

    // Estimate the transmission map
    Mat transmission = 1 - omega * (dark_channel / A[0]); // Correct division using A[0]
    transmission = max(transmission, t_min); // Ensure minimum transmission

    // Recover the scene radiance
    Mat J = Mat::zeros(img.size(), CV_32FC3); // Use CV_32FC3 for float operations
    split(img_float, channels); // Use img_float instead of img
    for (int c = 0; c < 3; c++) {
        channels[c] = (channels[c] - A[0]) / transmission + A[0]; // Adjust each channel
    }
    merge(channels, J); // Merge channels back into an image

    // Clip the values to [0, 1] range
    J = min(max(J, 0), 1); // Ensure values are within [0, 1]

    // Convert back to 8-bit for display
    J.convertTo(J, CV_8UC3, 255); // Convert to 8-bit and scale to [0, 255]

    return J;
}

Mat enhanceUnderwaterFrame(const Mat& frame) {
    Mat img_wb = whiteBalance(frame);
    Mat img_he = histogramEqualization(img_wb);
    Mat img_de = dehaze(img_he);
    //Mat img_dehazed = dehaze(img_he);

    return img_he;
}



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

    camRaw = ui->cam_raw;
    camProc = ui->cam_proc;
    controllerStatusLabel = ui->label_controller_status;
    tempStatusLabel= ui->label_temp_status;
    pressureStatusLabel= ui->label_pressure_status;
    accStatusLabel= ui->label_acc_status;
    gravityStatusLabel= ui->label_gravity_status;
    gyroStatusLabel= ui->label_gyro_status;
    magStatusLabel= ui->label_mag_status;
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::startCamera);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopCamera);

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

    cap.open(0);

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

    camRaw->clear();
    camProc->clear();
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
    Mat enhancedFrame = enhanceUnderwaterFrame(frame);
    QImage qimgfinal(enhancedFrame.data, enhancedFrame.cols, enhancedFrame.rows, enhancedFrame.step, QImage::Format_RGB888);

    camRaw->setPixmap(QPixmap::fromImage(qimg));
    camProc->setPixmap(QPixmap::fromImage(qimgfinal));
}

void MainWindow::readXboxController()
{
    DWORD result = XInputGetState(0, &controllerState);
    if (result == ERROR_SUCCESS) {
        controllerStatusLabel->setText("Controller connected");
        processControllerInput();
    } else {
        controllerStatusLabel->setText("Controller not found or not connected");
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

    if (pad->wButtons & XINPUT_GAMEPAD_A) {
        resetThrusterValues();
        sendThrusterData();
    }

    // else{
    //     resetThrusterValues();
    //     sendThrusterData();
    // }
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
        datagram.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(datagram.data(), datagram.size());

        // Convert datagram to QString
        QString message = QString::fromUtf8(datagram);

        // Split the message on every occurrence of "/"
        QStringList parts = message.split('/');

        // Use QMetaObject::invokeMethod to update UI in the main thread
        QMetaObject::invokeMethod(this, [this, parts]() {
                if (!parts.isEmpty()) {
                    if (parts.size() > 0) {
                        tempStatusLabel->setText(parts.at(0));
                    } else {
                        tempStatusLabel->clear();
                    }

                    if (parts.size() > 1) {
                        pressureStatusLabel->setText(parts.at(1));
                    } else {
                        pressureStatusLabel->clear();
                    }

                    if (parts.size() > 4) {
                        accStatusLabel->setText(parts.at(2) + ' ' + parts.at(3) + ' ' + parts.at(4));
                    } else {
                        accStatusLabel->clear();
                    }

                    if (parts.size() > 7) {
                        gravityStatusLabel->setText(parts.at(5) + ' ' + parts.at(6) + ' ' + parts.at(7));
                    } else {
                        gravityStatusLabel->clear();
                    }

                    if (parts.size() > 10) {
                        gyroStatusLabel->setText(parts.at(8) + ' ' + parts.at(9) + ' ' + parts.at(10));
                    } else {
                        gyroStatusLabel->clear();
                    }

                    if (parts.size() > 13) {
                        magStatusLabel->setText(parts.at(11) + ' ' + parts.at(12) + ' ' + parts.at(13));
                    } else {
                        magStatusLabel->clear();
                    }
                }

            }, Qt::QueuedConnection);
    }
}
