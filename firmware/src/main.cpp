/**
 * @file main.cpp
 *
 * @author Professor Boots (original)
 * @author Braidan Duffy (fork)
 * @brief 
 * @version 2.0.0
 * @date 2023-11-12
 * 
 * @copyright Copyright (c) 2023
 * 
 * @
 */

#include <Arduino.h>
#ifdef ESP32
#include <SPIFFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

#include <ESP32Servo.h> //by Kevin Harrington
#include <iostream>
#include <sstream>

// #include "pins.h"

const char* ssid = "ProfBoots MiniSkidi";

struct MOTOR_PINS
{
  int pinIN1;
  int pinIN2;    
};

std::vector<MOTOR_PINS> motorPins = 
{
  {RIGHT_MOTOR_IN1, RIGHT_MOTOR_IN2},  //RIGHT_MOTOR Pins (IN1, IN2)
  {LEFT_MOTOR_IN1, LEFT_MOTOR_IN2},  //LEFT_MOTOR  Pins
  {ARM_MOTOR_IN1, ARM_MOTOR_IN2}, //ARM_MOTOR pins 
};

Servo bucketServo;
Servo auxServo;

#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define ARMUP 5
#define ARMDOWN 6
#define STOP 0

#define RIGHT_MOTOR 1
#define LEFT_MOTOR 0
#define ARM_MOTOR 2

#define FORWARD 1
#define BACKWARD -1

bool horizontalScreen;//When screen orientation is locked vertically this rotates the D-Pad controls so that forward would now be left.
bool removeArmMomentum = false;

AsyncWebServer server(80);
AsyncWebSocket wsCarInput("/CarInput");

void rotateMotor(int motorNumber, int motorDirection) {
    if (motorDirection == FORWARD) {
        digitalWrite(motorPins[motorNumber].pinIN1, HIGH);
        digitalWrite(motorPins[motorNumber].pinIN2, LOW);    
    }
    else if (motorDirection == BACKWARD) {
        digitalWrite(motorPins[motorNumber].pinIN1, LOW);
        digitalWrite(motorPins[motorNumber].pinIN2, HIGH);     
    }
    else {
        if(removeArmMomentum) {
            digitalWrite(motorPins[ARM_MOTOR].pinIN1, HIGH);
            digitalWrite(motorPins[ARM_MOTOR].pinIN2, LOW); 
            delay(10);
            digitalWrite(motorPins[motorNumber].pinIN1, LOW);
            digitalWrite(motorPins[motorNumber].pinIN2, LOW);
            delay(5);
            digitalWrite(motorPins[ARM_MOTOR].pinIN1, HIGH);
            digitalWrite(motorPins[ARM_MOTOR].pinIN2, LOW);
            delay(10);  
            removeArmMomentum = false;
        }
        digitalWrite(motorPins[motorNumber].pinIN1, LOW);
        digitalWrite(motorPins[motorNumber].pinIN2, LOW);       
    }
}

void moveCar(int inputValue) {
    Serial.printf("Got value as %d\n", inputValue); 
    if(!(horizontalScreen)) { 
        switch(inputValue) {
            case UP:
                rotateMotor(RIGHT_MOTOR, FORWARD);
                rotateMotor(LEFT_MOTOR, FORWARD);                  
                break;
        
            case DOWN:
                rotateMotor(RIGHT_MOTOR, BACKWARD);
                rotateMotor(LEFT_MOTOR, BACKWARD);  
                break;
        
            case LEFT:
                rotateMotor(RIGHT_MOTOR, BACKWARD);
                rotateMotor(LEFT_MOTOR, FORWARD);  
                break;
        
            case RIGHT:
                rotateMotor(RIGHT_MOTOR, FORWARD);
                rotateMotor(LEFT_MOTOR, BACKWARD); 
                break;
        
            case STOP:
                rotateMotor(ARM_MOTOR, STOP); 
                rotateMotor(RIGHT_MOTOR, STOP);
                rotateMotor(LEFT_MOTOR, STOP);    
                break;

            case ARMUP:
                rotateMotor(ARM_MOTOR, FORWARD);
                break;
            
            case ARMDOWN:
                rotateMotor(ARM_MOTOR, BACKWARD);
                removeArmMomentum = true;
                break; 
            
            default:
                rotateMotor(ARM_MOTOR, STOP);    
                rotateMotor(RIGHT_MOTOR, STOP);
                rotateMotor(LEFT_MOTOR, STOP); 
                break;
        }
    }
    else {
        switch(inputValue){
            case UP:
                rotateMotor(RIGHT_MOTOR, BACKWARD);
                rotateMotor(LEFT_MOTOR, FORWARD);                  
                break;
        
            case DOWN:
                rotateMotor(RIGHT_MOTOR, FORWARD);
                rotateMotor(LEFT_MOTOR, BACKWARD);  
                break;
        
            case LEFT:
                rotateMotor(RIGHT_MOTOR, BACKWARD);
                rotateMotor(LEFT_MOTOR, BACKWARD);  
                break;
        
            case RIGHT:
                rotateMotor(RIGHT_MOTOR, FORWARD);
                rotateMotor(LEFT_MOTOR, FORWARD); 
                break;
        
            case STOP:
                rotateMotor(ARM_MOTOR, STOP); 
                rotateMotor(RIGHT_MOTOR, STOP);
                rotateMotor(LEFT_MOTOR, STOP);    
                break;

            case ARMUP:
                rotateMotor(ARM_MOTOR, FORWARD); 
                break;
            
            case ARMDOWN:
                rotateMotor(ARM_MOTOR, BACKWARD);
                removeArmMomentum = true;
                break; 
            
            default:
                rotateMotor(ARM_MOTOR, STOP);    
                rotateMotor(RIGHT_MOTOR, STOP);
                rotateMotor(LEFT_MOTOR, STOP); 
                break;
        }
    }
}


void bucketTilt(int bucketServoValue) {
    bucketServo.write(bucketServoValue); 
}

void auxControl(int auxServoValue) {
    auxServo.write(auxServoValue); 
}


void handleRoot(AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", String(), false);
}

void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "File Not Found");
}

void onCarInputWebSocketEvent(AsyncWebSocket *server, 
                            AsyncWebSocketClient *client, 
                            AwsEventType type,
                            void *arg, 
                            uint8_t *data, 
                            size_t len) {                      
    switch (type) {
        case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
        case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        moveCar(STOP);
        break;
        case WS_EVT_DATA:
            AwsFrameInfo *info;
            info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                std::string myData = "";
                myData.assign((char *)data, len);
                std::istringstream ss(myData);
                std::string key, value;
                std::getline(ss, key, ',');
                std::getline(ss, value, ',');
                Serial.printf("Key [%s] Value[%s]\n", key.c_str(), value.c_str()); 
                int valueInt = atoi(value.c_str());     
                if (key == "MoveCar") {
                    moveCar(valueInt);        
                }
                else if (key == "AUX") {
                    auxControl(valueInt);
                }
                else if (key == "Bucket") {
                    bucketTilt(valueInt);        
                }  
                else if (key =="Switch") {
                    if(!(horizontalScreen)) {
                        horizontalScreen = true;   
                    }
                    else {
                        horizontalScreen = false;
                    }
                }
            }
            break;

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;

        default:
            break;  
    }
}


void setup(void)  {
    Serial.begin(115200);

    // Initialize SPIFFS
    if(!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    // Initialize motors and servos
    moveCar(STOP);
    bucketServo.attach(BUCKET_SERVO_PIN);
    auxServo.attach(AUX1_SERVO_PIN);
    auxControl(150);
    bucketTilt(140);

    WiFi.softAP(ssid );
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleNotFound);
        
    wsCarInput.onEvent(onCarInputWebSocketEvent);
    server.addHandler(&wsCarInput);

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    wsCarInput.cleanupClients(); 
}
