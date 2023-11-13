#ifndef DRV8833_H
#define DRV8833_H

#include <Arduino.h>

class DRV8833 {
    public:
        DRV8833(uint8_t in1, uint8_t in2, bool removeMomentum=false, bool reversePins=false);

        void forward() { _setBridgePins(HIGH, LOW); };
        void backward() { _setBridgePins(LOW, HIGH); }
        void stop();

    private:
        uint8_t pinIn1;
        uint8_t pinIn2;
        bool removeMomentum;
        bool reversePins;

        void _removeMomentum();
        void _setBridgePins(bool in1, bool in2);
};

extern DRV8833 rightMotor;
extern DRV8833 leftMotor;
extern DRV8833 armMotor;
#endif // DRV8833_H