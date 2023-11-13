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
#include "motor_defs.h"
#include <DRV8833.h>

const char* ssid = "ProfBoots MiniSkidi";

Servo bucketServo;
Servo auxServo;

AsyncWebServer server(80);
AsyncWebSocket wsCarInput("/CarInput");

void stop() {
    rightMotor.stop();
    leftMotor.stop();
    armMotor.stop();
}

void move(int input) {
    Serial.printf("Got: %d\n\r", input);

    switch(input) {
        case UP:
            Serial.println("Going forward");
            rightMotor.forward();
            leftMotor.forward();
            break;
    
        case DOWN:
            rightMotor.backward();
            leftMotor.backward();
            break;
    
        case LEFT:
            rightMotor.forward();
            leftMotor.backward();
            break;
    
        case RIGHT:
            rightMotor.backward();
            leftMotor.forward();
            break;
    
        case STOP:
            rightMotor.stop();
            leftMotor.stop();
            armMotor.stop();
            break;

        case ARMUP:
            armMotor.forward();
            break;
        
        case ARMDOWN:
            armMotor.backward();
            break; 
        
        default:
            rightMotor.stop();
            leftMotor.stop();
            armMotor.stop();
            break;
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
        move(STOP);
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
                    move(valueInt);        
                }
                else if (key == "AUX") {
                    auxControl(valueInt);
                }
                else if (key == "Bucket") {
                    bucketTilt(valueInt);        
                }  
                // else if (key =="Switch") {
                //     if(!(horizontalScreen)) {
                //         horizontalScreen = true;   
                //     }
                //     else {
                //         horizontalScreen = false;
                //     }
                // }
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

    Serial.printf("RIGHT_MOTOR_REVERSE_PINS: %u\r\n", RIGHT_MOTOR_REVERSE_PINS);
    Serial.printf("LEFT_MOTOR_REVERSE_PINS: %u\r\n", LEFT_MOTOR_REVERSE_PINS);
    Serial.printf("ARM_MOTOR_REVERSE_PINS: %u\r\n", ARM_MOTOR_REVERSE_PINS);

    // Initialize SPIFFS
    if(!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    // Initialize motors and servos
    move(STOP);
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
