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
#include <ArduinoJson.h>

Servo bucketServo;
Servo auxServo;

AsyncWebServer server(80);
AsyncWebSocket wsCarInput("/CarInput");

struct Config {
    String ssid;
    String deviceName;
    bool reverseLeft;
    bool reverseRight;
    bool reverseArm;
};

Config config;

void loadConfig() {
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        configFile.close();

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, buf.get());

        if (!error) {
            config.ssid = doc["ssid"].as<String>();
            config.deviceName = doc["deviceName"].as<String>();
            config.reverseLeft = doc["reverseLeft"].as<bool>();
            config.reverseRight = doc["reverseRight"].as<bool>();
            config.reverseArm = doc["reverseArm"].as<bool>();
        }
    }
}

void saveConfig() {
    DynamicJsonDocument doc(1024);
    doc["ssid"] = config.ssid;
    doc["deviceName"] = config.deviceName;
    doc["reverseLeft"] = config.reverseLeft;
    doc["reverseRight"] = config.reverseRight;
    doc["reverseArm"] = config.reverseArm;

    File configFile = SPIFFS.open("/config.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
    }
}

void handleConfig(AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/config.html", String(), false);
} 

void handleSaveConfig(AsyncWebServerRequest *request) {
    String ssid = request->arg("ssid");
    String deviceName = request->arg("deviceName");
    bool reverseLeft = request->hasArg("reverseLeft");
    bool reverseRight = request->hasArg("reverseRight");
    bool reverseArm = request->hasArg("reverseArm");

    config.ssid = ssid;
    config.deviceName = deviceName;
    config.reverseLeft = reverseLeft;
    config.reverseRight = reverseRight;
    config.reverseArm = reverseArm;

    saveConfig();

    request->redirect("/");
}

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

    // Initialize SPIFFS
    if(!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    loadConfig();
    Serial.printf("SSID: %s\r\n", config.ssid);
    Serial.printf("Device Name: %s\r\n", config.deviceName);
    Serial.printf("Reverse Left: %s\r\n", config.reverseLeft ? "true" : "false");
    Serial.printf("Reverse Right: %s\r\n", config.reverseRight ? "true" : "false");
    Serial.printf("Reverse Arm: %s\r\n", config.reverseArm ? "true" : "false");

    // Initialize motors and servos
    if (config.reverseRight) { rightMotor.reversePins(); }
    if (config.reverseLeft) { leftMotor.reversePins(); }
    if (config.reverseArm) { armMotor.reversePins(); }

    move(STOP);
    bucketServo.attach(BUCKET_SERVO_PIN);
    auxServo.attach(AUX1_SERVO_PIN);
    auxControl(150);
    bucketTilt(140);

    WiFi.softAP(config.ssid.c_str());
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleNotFound);
    server.on("/config", handleConfig);
    server.on("/saveConfig", HTTP_POST, handleSaveConfig);
        
    wsCarInput.onEvent(onCarInputWebSocketEvent);
    server.addHandler(&wsCarInput);

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    wsCarInput.cleanupClients(); 
}
