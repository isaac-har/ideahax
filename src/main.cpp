#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <bme68x.h>
#include <bme68x_defs.h>
#include <Adafruit_BME680.h>
#include <Wire.h>
#include <SparkFun_VEML6030_Ambient_Light_Sensor.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebSocketsServer.h> // New dependency for wireless telemetry
#include <GP2YDustSensor.h>
#include <ESP32Servo.h>

// Networking Configuration
const char *ssid = "WeatherStation_AP";
const char *password = "ideahacks";
WebSocketsServer webSocket = WebSocketsServer(81); // WebSocket server on port 81

Adafruit_BME680 bme;

// AMBIENT LIGHT SENSOR CODE BELOW:
#define AL_ADDR 0x48
#define I2C_SDA 6
#define I2C_SCL 5

// WATER SENSOR PIN DECLARATIONS:
#define waterSensorPower 7
#define waterSensorPin 3
int waterLevel = 0;

// Possible values: .125, .25, 1, 2
// Both .125 and .25 should be used in most cases except darker rooms.
// A gain of 2 should only be used if the sensor will be covered by a dark
// glass.
float gain = .125;

// Possible integration times in milliseconds: 800, 400, 200, 100, 50, 25
// Higher times give higher resolutions and should be used in darker light.
int lightTime = 100;
long luxVal = 0;

SparkFun_Ambient_Light light(AL_ADDR);

// SPI Pins
#define VSPI_MISO 13
#define VSPI_MOSI 11
#define VSPI_SCLK 12
#define VSPI_SS 10
SPIClass *sdSPI = new SPIClass(FSPI);

File sdDataFile;

// DUST SENSOR CODE BELOW:
const uint8_t SHARP_LED_PIN = 14; // Sharp Dust/particle sensor Led Pin
const uint8_t SHARP_VO_PIN = 4;   // Sharp Dust/particle analog out pin used for reading

GP2YDustSensor dustSensor(GP2YDustSensorType::GP2Y1010AU0F, SHARP_LED_PIN, SHARP_VO_PIN);

// Servo Setup
Servo myServo;
#define SERVO_PIN 8
float maxLux = 0;
int bestAngle = 0;

// Non blocking timer
unsigned long previousMillis = 0;
const long servoInterval = 60000;
unsigned long currentMillis = 50000;

//Solar Panel
const int SOLAR_PIN = 1; // GPIO 1 (ADC1_CH0)
int rawSolarADC = 0;
float solarPinVoltage = 0;

void servoSweep()
{
  maxLux = -1.0;
  bestAngle = 0;

  for (int pos = 0; pos <= 180; pos += 2)
  { // Move in 2-degree increments for speed
    myServo.write(pos);

    // We must wait at least the 'lightTime' (100ms) for the sensor
    // to complete a reading at this new position.
    delay(lightTime + 20);

    float currentLux = light.readLight();

    if (currentLux > maxLux)
    {
      maxLux = currentLux;
      bestAngle = pos;
    }

    // Optional: Print progress
    if (pos % 20 == 0)
      Serial.print(".");
  }

  Serial.println("\nScan Complete!");
  Serial.printf("Brightest Light: %.2f lux at %d degrees\n", maxLux, bestAngle);

  // Return to the best position
  Serial.println("Targeting brightest spot...");
  myServo.write(bestAngle);
}


void setup()
{

  Serial.begin(115200);

  delay(2000);
  Wire.begin(I2C_SDA, I2C_SCL); // SDA, SCL pins

  bool found = false;
  for (byte address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0)
    {
      Serial.print("Found device at 0x");
      Serial.println(address, HEX);
      found = true;
      delay(10);
    }
  }

  // Initialize Wi-Fi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Initialize WebSocket Server
  webSocket.begin();
  Serial.println("WebSocket Server Started on Port 81");

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!bme.begin(0x77))
  {
    Serial.println(F("Could not find a valid BME680 sensor!"));
    while (1)
      ;
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  // Light sensor setup
  if (light.begin())
    Serial.println("Ready to sense some light!");
  else
    Serial.println("Could not communicate with the sensor!");

  light.setGain(gain);
  light.setIntegTime(lightTime);

  Serial.println("Light Sensor Settings:");
  Serial.print("Gain: ");
  float gainVal = light.readGain();
  Serial.print(gainVal, 3);
  Serial.print(" Integration Time: ");
  int timeVal = light.readIntegTime();
  Serial.println(timeVal);

  // SD Card setup
  sdSPI->begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS);

  if (!SD.begin(VSPI_SS, *sdSPI))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  Serial.println("SD Card Initialized");

  // Dust sensor setup

  // dustSensor.setBaseline(0.4); // set no dust voltage according to your own experiments
  // dustSensor.setCalibrationFactor(1.1); // calibrate against precision instrument
  dustSensor.begin();

  // Water sensor setup
  pinMode(waterSensorPower, OUTPUT);
  digitalWrite(waterSensorPower, LOW); // Power the water sensor

  // Servo setup
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
}

void loop()
{
  // Keep the WebSocket server alive
  webSocket.loop();

  float temp = 0, hum = 0, press = 0, gas = 0, lux = 0, dust = 0;

  if (bme.performReading())
  {
    temp = bme.temperature;
    gas = bme.gas_resistance / 1000.0;
    hum = bme.humidity;
    press = bme.pressure / 100.0;
  }
  lux = light.readLight();

  dust = dustSensor.getDustDensity();

  digitalWrite(waterSensorPower, HIGH); // Turn the sensor ON
  delay(10);                            // wait 10 milliseconds
  waterLevel = analogRead(waterSensorPin);
  digitalWrite(waterSensorPower, LOW); // Turn the sensor OFF

  rawSolarADC = analogRead(SOLAR_PIN);
  solarPinVoltage = (rawSolarADC / 4095.0) * 3.3;
  solarPinVoltage = (solarPinVoltage * 3.0) + 0.18;

  // Serial Output
  Serial.printf("Temperature: %.2f C\n Humidity: %.2f %%\n Pressure: %.2f hPa\n Gas: %.2f kΩ\n Light: %.2f lux\n Dust: %.2f ug/m3\n Water Level: %d\n Solar Voltage: %.2f V\n\n", temp, hum, press, gas, lux, dust, waterLevel, solarPinVoltage);

  // WebSocket Broadcast
  // We package the data into a JSON string for the Web UI
  String json = "{";
  json += "\"temp\":" + String(temp) + ",";
  json += "\"hum\":" + String(hum) + ",";
  json += "\"press\":" + String(press) + ",";
  json += "\"gas\":" + String(gas) + ",";
  json += "\"lux\":" + String(lux) + ",";
  json += "\"dust\":" + String(dust) + ",";
  json += "\"water\":" + String(waterLevel) + ",";
  json += "\"solar\":" + String(solarPinVoltage);
  json += "}";

  webSocket.broadcastTXT(json); // Push data to all connected browsers

  // SD Logging (Preserved from friend's code)
  sdDataFile = SD.open("/sdData.txt", FILE_APPEND); // Use APPEND to keep history
  if (sdDataFile)
  {
    sdDataFile.printf("Temperature: %.2f C, Humidity: %.2f %%, Pressure: %.2f hPa, Gas: %.2f kΩ, Light: %.2f lux, Dust: %.2f ug/m3, Water Level: %d, Solar Voltage: %.2f V\n", temp, hum, press, gas, lux, dust, waterLevel, solarPinVoltage);
    sdDataFile.close();
  }

  currentMillis = millis();

  if (currentMillis - previousMillis >= servoInterval)
  {
    previousMillis = currentMillis;
    servoSweep(); 
  }

  delay(990);
}

