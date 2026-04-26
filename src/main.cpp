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
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>
#include <Fonts/FreeSans9pt7b.h>  // Include a 9pt font

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
const long servoInterval = 30000;
unsigned long currentMillis = 0;

// Solar Panel
const int SOLAR_PIN = 1; // GPIO 1 (ADC1_CH0)
int rawSolarADC = 0;
float solarPinVoltage = 0;

// Non blocking timer for servo sweep
bool isSweeping = false; // Keeps track of whether a sweep is active
int currentSweepPos = 0; // Replaces the 'pos' variable from your for-loop
unsigned long previousMillisServo = 0;
const long sweepDelay = lightTime + 20; // lightTime   (100) + 20ms
float currentMaxLux = -1.0;
int currentBestAngle = 0;

// Main loop interval sensor rate limiting
const long mainInterval = 1000; // 1 second update rate for main sensors
unsigned long previousMillisMain = 0;

// OLED Display Setup
#define OLED_RESET -1 // No reset pin
#define SCREEN_ADDRESS 0x3D
Adafruit_SSD1327 display(128, 128, &Wire, OLED_RESET);

void updateServoSweep()
{
  // 1. If we aren't currently sweeping, exit the function immediately
  if (!isSweeping)
    return;

  unsigned long currentMillis = millis();

  // 2. Has 120ms passed since our last step?
  if (currentMillis - previousMillisServo >= sweepDelay)
  {
    previousMillisServo = currentMillis; // Reset the timer

    // 3. Take a reading for the CURRENT position
    float currentLux = light.readLight();

    if (currentLux > currentMaxLux)
    {
      currentMaxLux = currentLux;
      currentBestAngle = currentSweepPos;
    }

    // 4. Increment position for the NEXT step
    currentSweepPos += 2;

    // 5. Move the servo, OR end the sweep if we reached 180
    if (currentSweepPos <= 180)
    {
      myServo.write(currentSweepPos);
    }
    else
    {
      // We exceeded 180 degrees. The sweep is done.
      Serial.println("\nScan Complete!");
      Serial.printf("Brightest Light: %.2f lux at %d degrees\n", currentMaxLux, currentBestAngle);

      Serial.println("Targeting brightest spot...");
      myServo.write(currentBestAngle); // Move to the best spot

      isSweeping = false; // Turn off the state machine
    }
  }
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
  delay(1000);
  if (!SD.begin(VSPI_SS, *sdSPI))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  Serial.println("SD Card Initialized");

  // OLED setup
  if (!display.begin(SCREEN_ADDRESS))
  {
    Serial.println("SSD1327 not found. Check wiring/address!");
  }
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
  // Keep the WebSocket server alive (needs fast, continuous looping)
  webSocket.loop();
  currentMillis = millis();

  // 1. NON-BLOCKING TIMER FOR SENSORS (Replaces delay(1000))
  if (currentMillis - previousMillisMain >= mainInterval)
  {
    previousMillisMain = currentMillis;

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

    digitalWrite(waterSensorPower, HIGH);
    delay(10); // A 10ms delay is short enough not to disrupt the 120ms servo sweep
    waterLevel = analogRead(waterSensorPin);
    digitalWrite(waterSensorPower, LOW);

    rawSolarADC = analogRead(SOLAR_PIN);
    // printf("Raw Solar ADC: %d\n", rawSolarADC);

    solarPinVoltage = analogReadMilliVolts(SOLAR_PIN) / 1000.0; // Convert mV to V
    solarPinVoltage = (solarPinVoltage * 3.0);

    // Serial Output
    Serial.printf("Temperature: %.2f C\n Humidity: %.2f %%\n Pressure: %.2f hPa\n Gas: %.2f kΩ\n Light: %.2f lux\n Dust: %.2f ug/m3\n Water Level: %d\n Solar Voltage: %.2f V\n\n", temp, hum, press, gas, lux, dust, waterLevel, solarPinVoltage);

    // WebSocket Broadcast
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

    webSocket.broadcastTXT(json);

    // SD Logging
    sdDataFile = SD.open("/sdData.txt", FILE_APPEND);
    if (sdDataFile)
    {
      sdDataFile.printf("Temperature: %.2f C, Humidity: %.2f %%, Pressure: %.2f hPa, Gas: %.2f kΩ, Light: %.2f lux, Dust: %.2f ug/m3, Water Level: %d, Solar Voltage: %.2f V\n", temp, hum, press, gas, lux, dust, waterLevel, solarPinVoltage);
      sdDataFile.close();
    }

    display.clearDisplay();
    // display.setFont(&FreeSans9pt7b);
    display.setTextSize(2);
    display.setTextColor(0xF);
    display.setCursor(0, 0);
    display.printf("Tmp: %.1fC\nHum: %.1f%%\nPre: %.1f\nGas: %.1f\nLux: %.1f\nDst: %.0f\nWtr: %d\nSlr: %.2fV", temp, hum, press, gas, lux, dust, waterLevel, solarPinVoltage);
    display.display();
  }

  // 2. TRIGGER SERVO SWEEP EVERY 30 SECONDS
  if (currentMillis - previousMillis >= servoInterval)
  {
    previousMillis = currentMillis;

    isSweeping = true;
    currentSweepPos = 0;
    currentMaxLux = -1.0;
    myServo.write(0);
    previousMillisServo = millis();
  }

  // 3. EXECUTE SERVO STATE MACHINE
  updateServoSweep();
}
