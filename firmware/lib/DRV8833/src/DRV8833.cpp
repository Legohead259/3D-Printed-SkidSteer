#include <DRV8833.h>

DRV8833::DRV8833(uint8_t in1, uint8_t in2, bool removeMomentum) {
    pinIn1 = in1;
    pinIn2 = in2;
    removeMomentum = removeMomentum;

    pinMode(pinIn1, OUTPUT);
    pinMode(pinIn2, OUTPUT);
}

void DRV8833::stop() {
    if (removeMomentum) { _removeMomentum(); }
    _setBridgePins(LOW, LOW);
}

void DRV8833::reversePins() {
    uint8_t _temp = pinIn1;
    pinIn1 = pinIn2;
    pinIn2 = _temp;
}

void DRV8833::_removeMomentum() {
    _setBridgePins(HIGH, LOW);
    delay(10);
    _setBridgePins(LOW, LOW);
    delay(5);
    _setBridgePins(HIGH, LOW);
    delay(10);  
}

void DRV8833::_setBridgePins(bool in1, bool in2) {
    digitalWrite(pinIn1, in1);
    digitalWrite(pinIn2, in2);
}

DRV8833 rightMotor(RIGHT_MOTOR_IN1, RIGHT_MOTOR_IN2, false);
DRV8833 leftMotor(LEFT_MOTOR_IN1, LEFT_MOTOR_IN2, false);
DRV8833 armMotor(ARM_MOTOR_IN1, ARM_MOTOR_IN2, true);