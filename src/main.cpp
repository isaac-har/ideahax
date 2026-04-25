#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <bme68x.h>
#include <bme68x_defs.h>
#include <Adafruit_BME680.h>
#include <Wire.h>
#include <SparkFun_VEML6030_Ambient_Light_Sensor.h>
#include <SPI.h>
#include <SD.h>
#include <GP2YDustSensor.h>

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

// SPI Pins
#define VSPI_MISO 13
#define VSPI_MOSI 11
#define VSPI_SCLK 12
#define VSPI_SS 10
SPIClass *sdSPI = new SPIClass(FSPI);

// SD Card file object
File sdDataFile;

// DUST SENSOR CODE BELOW:
const uint8_t SHARP_LED_PIN = 14; // Sharp Dust/particle sensor Led Pin
const uint8_t SHARP_VO_PIN = 4;   // Sharp Dust/particle analog out pin used for reading

GP2YDustSensor dustSensor(GP2YDustSensorType::GP2Y1010AU0F, SHARP_LED_PIN, SHARP_VO_PIN);

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
}

void loop()
{

  if (bme.performReading())
  {
    // BME680 sensor readings
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

  // Light sensor reading
  Serial.print("Ambient Light Reading: ");
  Serial.print(light.readLight());
  Serial.println(" Lux");
  Serial.println();

  // SD Writing
  sdDataFile = SD.open("/sdData.txt", FILE_WRITE);
  if (sdDataFile)
  {
    Serial.print("Writing to SD card...");

    sdDataFile.print("Temp: ");
    sdDataFile.print(bme.temperature);
    sdDataFile.println(" *C");
    sdDataFile.print("Gas: ");
    sdDataFile.print(bme.gas_resistance / 1000.0);
    sdDataFile.println(" KOhms");
    sdDataFile.print("Humidity: ");
    sdDataFile.println(bme.humidity);
    sdDataFile.print("Pressure: ");
    sdDataFile.println(bme.pressure / 100.0);

    sdDataFile.print("Ambient Light Reading: ");
    sdDataFile.print(light.readLight());
    sdDataFile.println(" Lux");
    sdDataFile.println();

    sdDataFile.print("Dust density: ");
    sdDataFile.print(dustSensor.getDustDensity());
    sdDataFile.print(" ug/m3; Running average: ");
    sdDataFile.print(dustSensor.getRunningAverage());
    sdDataFile.println(" ug/m3");

    sdDataFile.close(); // Close to save
    Serial.println("done.");
  }

  else
  {
    Serial.println("Error opening sdData.txt");
  }

  // SD Reading
  //  sdDataFile = SD.open("/sdData.txt");
  //  if (sdDataFile) {
  //    Serial.println("Reading /sdData.txt:");
  //    while (sdDataFile.available()) {
  //      Serial.write(sdDataFile.read());
  //    }
  //    sdDataFile.close();
  //  } else {
  //    Serial.println("Error opening /sdData.txt");
  //  }

  // Dust sensor reading
  Serial.print("Dust density: ");
  Serial.print(dustSensor.getDustDensity());
  Serial.print(" ug/m3; Running average: ");
  Serial.print(dustSensor.getRunningAverage());
  Serial.println(" ug/m3");

  delay(2000);
}

// put function definitions here:
