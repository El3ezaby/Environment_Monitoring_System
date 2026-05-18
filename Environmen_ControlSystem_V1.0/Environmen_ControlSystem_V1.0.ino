#include <WiFi.h>
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <LiquidCrystal_I2C.h>

// WiFi & UDP
#define WIFI_SSID   "El3ezaby"
#define WIFI_PASS   "10000000@#"
WiFiUDP udp;
#define UDP_PORT    5005
#define ESP_IP      "192.168.1.2"
// Pins
#define DHT_PIN     15
#define FAN_PIN     2
#define HEATER_PIN  4

// L298N Motor Driver Pins (for Peltier)
#define PELTIER_PWM 13
#define PELTIER_DIR 12

#define SD_CS 5

// EEPROM Addresses
#define TEMP_ADDR 0
#define HUM_ADDR 4

#define ADS115_1_ADDR  0x48
#define ADS115_2_ADDR  0x49
#define ADS115_3_ADDR  0x4A
#define ADS115_4_ADDR  0x4B
// Constants for MQ Sensors
#define RL 4.7 // Load resistor value in kOhms
float R0[9] = {10, 10, 10, 10, 10, 10, 10, 10, 10}; // Default R0 values (calibration required)
float A[9] = {116.602, 102.2, 200, 600, 100, 110, 105, 100, 150};
float B[9] = {-2.769, -2.5, -2.3, -2.0, -2.2, -2.1, -2.0, -2.3, -2.5};
float mq_ppm[9];
// Variables
float temperature, humidity;
float mq_values[9];  // Stores all MQ sensor readings
int temp_threshold, hum_threshold;
File logFile;

// Initialize three ADS1115 modules
Adafruit_ADS1115 ads1;  // First ADS (ADDR = GND)
Adafruit_ADS1115 ads2;  // Second ADS (ADDR = VCC)
Adafruit_ADS1115 ads3;  // Third ADS (ADDR = SDA)

LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD instance (I2C address 0x27)
int displayIndex = 0;

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");

    udp.begin(UDP_PORT);

    // Initialize ADS1115 Modules
    if (!ads1.begin(ADS115_1_ADDR)) Serial.println("ADS1115 (0x48) not found!");
    if (!ads2.begin(ADS115_2_ADDR)) Serial.println("ADS1115 (0x49) not found!");
    if (!ads3.begin(ADS115_3_ADDR)) Serial.println("ADS1115 (0x4A) not found!");
    
    ads1.setGain(GAIN_ONE);
    ads2.setGain(GAIN_ONE);
    ads3.setGain(GAIN_ONE);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("ESP32 Sensor Init");
    delay(2000);
    lcd.clear();

    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);

    pinMode(HEATER_PIN, OUTPUT);
    digitalWrite(HEATER_PIN, LOW);

    pinMode(PELTIER_PWM, OUTPUT);
    pinMode(PELTIER_DIR, OUTPUT);

    EEPROM.begin(8);
    temp_threshold = EEPROM.read(TEMP_ADDR);
    hum_threshold = EEPROM.read(HUM_ADDR);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card initialization failed!");
        return;
    }
    Serial.println("SD Card initialized.");
}

void loop()
{
    readSensors();
    logAndSendData();
    rotateLCDDisplay();

  if (udp.parsePacket()) {
      char buffer[20];  // Buffer for received data
      int len = udp.read(buffer, sizeof(buffer) - 1);
      buffer[len] = '\0';  // Null-terminate the string

      // Remove the trailing '*' if it exists
      char *star_pos = strchr(buffer, '*');
      if (star_pos) {
          *star_pos = '\0';  // Replace '*' with null terminator
      }

      // Extract temperature and humidity using strtok
      char *tempStr = strtok(buffer, ",");
      char *humStr = strtok(NULL, ",");

      if (tempStr && humStr) {
          float temp_threshold = atof(tempStr);
          float hum_threshold = atof(humStr);

          EEPROM.put(TEMP_ADDR, temp_threshold);
          EEPROM.put(HUM_ADDR, hum_threshold);
          EEPROM.commit();
      }
  }


    controlTemperatureHumidity();
    delay(1000);
}

// Read sensors
void readSensors() {
    temperature = random(20, 40);
    humidity = random(30, 80);
    int raw_values[9] = {
        ads1.readADC_SingleEnded(0), ads1.readADC_SingleEnded(1), ads1.readADC_SingleEnded(2), ads1.readADC_SingleEnded(3),
        ads2.readADC_SingleEnded(0), ads2.readADC_SingleEnded(1), ads2.readADC_SingleEnded(2), ads2.readADC_SingleEnded(3),
        ads3.readADC_SingleEnded(0)
    };
    // Read MQ sensors from ADS1
    // mq_values[0] = ads1.readADC_SingleEnded(0); // MQ2
    // mq_values[1] = ads1.readADC_SingleEnded(1); // MQ3
    // mq_values[2] = ads1.readADC_SingleEnded(2); // MQ4
    // mq_values[3] = ads1.readADC_SingleEnded(3); // MQ5

    // // Read MQ sensors from ADS2
    // mq_values[4] = ads2.readADC_SingleEnded(0); // MQ6
    // mq_values[5] = ads2.readADC_SingleEnded(1); // MQ7
    // mq_values[6] = ads2.readADC_SingleEnded(2); // MQ8
    // mq_values[7] = ads2.readADC_SingleEnded(3); // MQ9

    // // Read MQ135 from ADS3
    // mq_values[8] = ads3.readADC_SingleEnded(0); // MQ135

    for (int i = 0; i < 9; i++) {
        float voltage = (raw_values[i] / 65535.0) * 3.3; /// 2 ^16  65536
        float Rs = ((3.3 * RL) / voltage) - RL;
        mq_values[i] = A[i] * pow(Rs / R0[i], B[i]); // ppm
    }
}

// Log and send data
void logAndSendData() {
    String dataString = "Temperature:" + String(temperature) + ",Humidity:" + String(humidity);
    
    for (int i = 0; i < 9; i++) {
        dataString += ",MQ" + String(i+2) + ":" + String(mq_values[i]);
    }

    Serial.println(dataString);
    
    logFile = SD.open("/sensor_log.csv", FILE_APPEND);
    if (logFile) {
        logFile.println(dataString);
        logFile.close();
    }

    udp.beginPacket(ESP_IP, UDP_PORT);
    udp.print(dataString);
    udp.endPacket();
}

// Rotate LCD display
void rotateLCDDisplay() {
    lcd.clear();
    
    lcd.setCursor(0, 0);
    lcd.print("Tth: " + String(temp_threshold) + "C Hth: " + String(hum_threshold) + "%");

    int sensorIndex = displayIndex;
    lcd.setCursor(0, 1);
    if (sensorIndex == 0) {
        lcd.print("T: " + String(temperature) + " H: " + String(humidity));
    } else {
        lcd.print("MQ" + String(sensorIndex + 1) + ": " + String(mq_values[sensorIndex - 1]));
    }

    displayIndex = (displayIndex + 1) % 10;
}

// Control temperature & humidity
void controlTemperatureHumidity() {
    if (temperature > temp_threshold) {
        Serial.println("Cooling with Peltier and Fan...");
        
        digitalWrite(HEATER_PIN, LOW);
        digitalWrite(FAN_PIN, HIGH);
        
        digitalWrite(PELTIER_PWM, HIGH);
        digitalWrite(PELTIER_DIR, LOW);
        
    } else if (temperature < temp_threshold - 3) {
        Serial.println("Heating with Heater...");
        
        digitalWrite(FAN_PIN, LOW);
        digitalWrite(PELTIER_PWM, HIGH);
        digitalWrite(PELTIER_DIR, HIGH);
        digitalWrite(HEATER_PIN, HIGH);
        
    } else {
        Serial.println("Temperature is within safe range.");
        
        digitalWrite(FAN_PIN, LOW);
        digitalWrite(PELTIER_PWM, LOW);
        digitalWrite(PELTIER_DIR, LOW);
        digitalWrite(HEATER_PIN, LOW);
    }
}
