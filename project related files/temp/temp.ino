#include <WiFi.h>
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

// WiFi & UDP
#define WIFI_SSID "your_SSID"
#define WIFI_PASS "your_PASSWORD"
WiFiUDP udp;
#define UDP_PORT 5005

// Pins
#define DHT_PIN 4
#define MQ2_PIN 32
#define MQ3_PIN 33
#define MQ4_PIN 34
#define MQ5_PIN 35
#define MQ6_PIN 36
#define MQ7_PIN 39
#define MQ8_PIN 25
#define MQ9_PIN 26
#define MQ135_PIN 27
#define FAN_PIN 2
#define SD_CS 5

// EEPROM Addresses
#define TEMP_ADDR 0
#define HUM_ADDR 4

// Variables
float temperature, humidity;
float mq_values[9];
int temp_threshold, hum_threshold;
File logFile;

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");

    udp.begin(UDP_PORT);

    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);

    EEPROM.begin(8);
    temp_threshold = EEPROM.read(TEMP_ADDR);
    hum_threshold = EEPROM.read(HUM_ADDR);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card initialization failed!");
        return;
    }
    Serial.println("SD Card initialized.");
}

void loop() {
    readSensors();
    logAndSendData();

    if (udp.parsePacket()) {
        char buffer[10];
        udp.read(buffer, sizeof(buffer));
        sscanf(buffer, "%d,%d", &temp_threshold, &hum_threshold);
        EEPROM.write(TEMP_ADDR, temp_threshold);
        EEPROM.write(HUM_ADDR, hum_threshold);
        EEPROM.commit();
    }

    controlFan();
    delay(2000);
}

void readSensors() {
    // Simulating sensor readings
    temperature = random(20, 40);
    humidity = random(30, 80);
    
    mq_values[0] = analogRead(MQ2_PIN);
    mq_values[1] = analogRead(MQ3_PIN);
    mq_values[2] = analogRead(MQ4_PIN);
    mq_values[3] = analogRead(MQ5_PIN);
    mq_values[4] = analogRead(MQ6_PIN);
    mq_values[5] = analogRead(MQ7_PIN);
    mq_values[6] = analogRead(MQ8_PIN);
    mq_values[7] = analogRead(MQ9_PIN);
    mq_values[8] = analogRead(MQ135_PIN);
}

void logAndSendData() {
    String dataString = "Temperature:" + String(temperature) + ",Humidity:" + String(humidity);
    for (int i = 0; i < 9; i++) {
        dataString += ",MQ" + String(i+2) + ":" + String(mq_values[i]);
    }

    Serial.println(dataString);
    
    // Log to SD Card
    logFile = SD.open("/sensor_log.csv", FILE_APPEND);
    if (logFile) {
        logFile.println(dataString);
        logFile.close();
    }

    // Send UDP packet
    udp.beginPacket("192.168.1.2", UDP_PORT);  // Replace with PC IP
    udp.print(dataString);
    udp.endPacket();
}

void controlFan() {
    if (temperature > temp_threshold || humidity > hum_threshold) {
        digitalWrite(FAN_PIN, HIGH);
    } else {
        digitalWrite(FAN_PIN, LOW);
    }
}
