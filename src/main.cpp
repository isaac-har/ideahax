#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <bme68x.h>
#include <bme68x_defs.h>
#include <Adafruit_BME680.h>
#include <Wire.h>

Adafruit_BME680 bme;
// put function declarations here:

void setup()
{

  Serial.begin(115200);

  delay(2000);
  Wire.begin(12, 15); // SDA, SCL pins

  Serial.println("Hello, World!");

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
  }
  delay(2000);
}

// put function definitions here:
