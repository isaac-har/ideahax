#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <bme68x.h>
#include <bme68x_defs.h>
#include <Adafruit_BME680.h>
#include <Wire.h>
#include <SparkFun_VEML6030_Ambient_Light_Sensor.h>

Adafruit_BME680 bme;

// AMBIENT LIGHT SENSOR CODE BELOW:
#define AL_ADDR 0x48
#define I2C_SDA 6
#define I2C_SCL 5
SparkFun_Ambient_Light light(AL_ADDR);

// Possible values: .125, .25, 1, 2
// Both .125 and .25 should be used in most cases except darker rooms.
// A gain of 2 should only be used if the sensor will be covered by a dark
// glass.
float gain = .125;

// Possible integration times in milliseconds: 800, 400, 200, 100, 50, 25
// Higher times give higher resolutions and should be used in darker light.
int lightTime = 100;
long luxVal = 0;

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

  if (!found)
  {
    Serial.println("No I2C devices found");
  }
  if (!bme.begin(0x77))
  {
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
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
}

void loop()
{

  if (bme.performReading())
  {
    Serial.print("Temp: ");
    Serial.print(bme.temperature);
    Serial.println(" *C");
    Serial.print("Gas: ");
    Serial.print(bme.gas_resistance / 1000.0);
    Serial.println(" KOhms");
    Serial.print("Humidity: ");
    Serial.println(bme.humidity);
    Serial.print("Pressure: ");
    Serial.println(bme.pressure / 100.0);
    Serial.print("Ambient Light Reading: ");
    Serial.print(light.readLight());
    Serial.println(" Lux");
    Serial.println();
  }
  delay(2000);
}

// put function definitions here:
