#ifndef XBOXCONTROLLER_H
#define XBOXCONTROLLER_H

#include <QObject>
#include <XInput.h>

class XboxController : public QObject {
    Q_OBJECT

public:
    explicit XboxController(QObject *parent = nullptr);
    int read();

private:
    void surgeControl();
    void swayControl();
    void heaveControl();
    void rest();

    void initializeXInput();

    XINPUT_STATE controllerState;
    int controllerNum;
    float deadZoneX;
    float deadZoneY;

    static const int MAX_TRIG_VAL = 255;
    static const int MAX_JOY_VAL = 32767;
};

#endif // XBOXCONTROLLER_H
