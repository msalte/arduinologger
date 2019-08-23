#include <DS1302.h> // hardware timer (clock) http://www.rinkydinkelectronics.com/library.php?id=5
#include "Adafruit_MAX31855.h"
#include <Adafruit_MAX31865.h>
#include <SPI.h>
#include <SD.h>

#include <Nextion.h>

//---------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------
const int DELAY_IN_MILLIS = 500;
const bool SHOULD_PRINT_TO_MONITOR = true;

const String FILE_NAME_EXHAUST = "exhaust.txt";
const String FILE_NAME_MISC_TEMP = "misc.txt";
const String FILE_NAME_PRESSURE = "pressure.txt";

const double R_REF = 430.0;
const double R_NOMINAL = 100.0;

const double PRESSURE_REF_200_PSI = 3.5;
const double PRESSURE_REF_300_PSI = 5.15;
const double PRESSURE_REF_1600_PSI = 27;

const int CLK = 24;
const int CS = 25;

double maxValues[10];
// float mp[10];

//---------------------------------------------------------------------------------
// Global objects
//---------------------------------------------------------------------------------
Adafruit_MAX31855 cardExt1(CLK, CS, 30); // Adafruit_MAX31855(int8_t _sclk, int8_t _cs, int8_t _miso);
Adafruit_MAX31855 cardExt2(CLK, CS, 31);
Adafruit_MAX31855 cardExt3(CLK, CS, 32);
Adafruit_MAX31855 cardExt4(CLK, CS, 33);
Adafruit_MAX31855 cardExt5(CLK, CS, 34);
Adafruit_MAX31855 cardExt6(CLK, CS, 35);

Adafruit_MAX31855 cardTempBeforeInt(CLK, CS, 36);
Adafruit_MAX31855 cardTempAfterInt(CLK, CS, 37);

Adafruit_MAX31865 cardWaterTemp = Adafruit_MAX31865(CS, 38, 39, CLK);
Adafruit_MAX31865 cardOilTemp = Adafruit_MAX31865(CS, 40, 41, CLK);

//Adafruit_MAX31865 cardTempBeforeInt = Adafruit_MAX31865(CS, 41, 42, CLK);
//Adafruit_MAX31865 cardTempAfterInt = Adafruit_MAX31865(CS, 43, 44, CLK);

DS1302 rtc(16, 15, 14); // hardware timer (clock)  RST, DAT, CLK
File fileExhaust;
File fileMiscTemp;
File filePressure;

// nex, page, id, name
NexNumber nExt1(0, 36, "n26");
NexNumber nExt2(0, 37, "n27");
NexNumber nExt3(0, 38, "n28");
NexNumber nExt4(0, 39, "n29");
NexNumber nExt5(0, 40, "n30");
NexNumber nExt6(0, 41, "n31");

NexNumber nMaxExt1(0, 28, "n23");

NexButton bResetMax(0, 5, "b8");

NexText tMaxPressureOil(0, 42, "t22");
NexText tMaxPressureFuel(0, 43, "t23");
NexText tMaxPressureBeforeInt(0, 39, "t19");
NexText tMaxPressureAfterInt(0, 40, "t20");
NexText tMaxPressureManifold(0, 41, "t21");

NexTouch *nex_listen_list[] = {&bResetMax, NULL};

//---------------------------------------------------------------------------------
// Arduino Lifecycle
//---------------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);

  nexInit(115200);

  setupHardwareTime();

  bResetMax.attachPush(bResetMaxPushCallback);

  cardWaterTemp.begin(MAX31865_3WIRE);
  cardOilTemp.begin(MAX31865_3WIRE);
  //  cardTempBeforeInt.begin(MAX31865_3WIRE);
  //  cardTempAfterInt.begin(MAX31865_3WIRE);

  if (SHOULD_PRINT_TO_MONITOR)
  {
    Serial.println("-------------------------------------------------------------------------------");
    Serial.print("Started: ");
    Serial.println(rtc.getDateStr());
    Serial.println("-------------------------------------------------------------------------------");
    Serial.println();
  }

  setupSDCard();
}

bool shouldLogToSDCard()
{
  int sensorValue = analogRead(A0);
  float voltage = sensorValue * (5.0 / 1023.0);

  return voltage == 5.00;
}

// int pressureToAngle(float pressure)
// {
//   return (pressure * 18) % 360 - 18;
// }

void setDashboardValues(double ext1, double ext2, double ext3, double ext4, double ext5, double ext6, double t36, double t37, double tempWater, double tempOil,
                        float pressureBeforeInt, float pressureAfterInt, float pressureOilEngine, float pressureOilTurbo, float pressureWaterInject,
                        float pressureManifoil, float pressureFuel)
{
  nExt1.setValue(ext1);
  nExt2.setValue(ext2);
  nExt3.setValue(ext3);
  nExt4.setValue(ext4);
  nExt5.setValue(ext5);
  nExt6.setValue(ext6);

  if (ext3 > maxValues[0])
  {
    maxValues[0] = ext3;
    nMaxExt1.setValue(ext3);
  }

  // gPressureBeforeInt.setValue(pressureToAngle(pressureBeforeInt * 50));

  //  dtostrf(val, width, precision, buffer);

  char buffer[10];

  dtostrf(pressureOilTurbo, 5, 2, buffer);
  tMaxPressureOil.setText(buffer);

  dtostrf(pressureFuel, 5, 2, buffer);
  tMaxPressureFuel.setText(buffer);

  dtostrf(pressureBeforeInt, 5, 2, buffer);
  tMaxPressureBeforeInt.setText(buffer);

  dtostrf(pressureAfterInt, 5, 2, buffer);
  tMaxPressureAfterInt.setText(buffer);

  dtostrf(pressureManifoil, 5, 2, buffer);
  tMaxPressureManifold.setText(buffer);
}

void loop()
{
  double ext1 = cardExt1.readCelsius();
  double ext2 = cardExt2.readCelsius();
  double ext3 = cardExt3.readCelsius();
  double ext4 = cardExt4.readCelsius();
  double ext5 = cardExt5.readCelsius();
  double ext6 = cardExt6.readCelsius();

  double t36 = cardTempBeforeInt.readCelsius();
  double t37 = cardTempAfterInt.readCelsius();

  //  double tempBeforeInt = cardTempBeforeInt.temperature(R_NOMINAL, R_REF);
  //  double tempAfterInt = cardTempAfterInt.temperature(R_NOMINAL, R_REF);
  double tempWater = cardWaterTemp.temperature(R_NOMINAL, R_REF);
  double tempOil = cardWaterTemp.temperature(R_NOMINAL, R_REF);

  // pressure
  float pressureBeforeInt = getPressureInBar(A6, PRESSURE_REF_200_PSI);
  float pressureAfterInt = getPressureInBar(A7, PRESSURE_REF_200_PSI);
  float pressureOilEngine = getPressureInBar(A8, PRESSURE_REF_200_PSI);
  float pressureOilTurbo = getPressureInBar(A9, PRESSURE_REF_200_PSI);
  float pressureWaterInject = getPressureInBar(A4, PRESSURE_REF_1600_PSI);
  float pressureManifoil = getPressureInBar(A5, PRESSURE_REF_200_PSI);
  float pressureFuel = getPressureInBar(A6, PRESSURE_REF_200_PSI);

  setDashboardValues(ext1, ext2, ext3, ext4, ext5, ext6, t36, t37, tempWater, tempOil, pressureBeforeInt,
                     pressureAfterInt, pressureOilEngine, pressureOilTurbo, pressureWaterInject,
                     pressureManifoil, pressureFuel);

  if (shouldLogToSDCard() == true)
  {
    writeExhaustTemperatures(ext1, ext2, ext3, ext4, ext5, ext6);
    writeMiscTemperatures(tempWater, tempOil, t36, t37);
    writePressure(pressureBeforeInt, pressureAfterInt, pressureOilEngine, pressureOilTurbo, pressureWaterInject, pressureManifoil, pressureFuel);
  }

  if (SHOULD_PRINT_TO_MONITOR)
  {
    Serial.println("-------------------------------------------------------------------------------");
    Serial.print("Time: ");
    Serial.println(rtc.getTimeStr());
    Serial.println("-------------------------------------------------------------------------------");

    Serial.print("ext1: " + String(ext1) + "C");
    Serial.print(" | ext2: " + String(ext2) + "C");
    Serial.print(" | ext3: " + String(ext3) + "C");
    Serial.print(" | ext4: " + String(ext4) + "C");
    Serial.print(" | ext5: " + String(ext5) + "C");
    Serial.println(" | ext6: " + String(ext6) + "C");

    //    Serial.println("-------------------------------------------------------------------------------");
    //    Serial.print("Before int: " + String(t36) + "C");
    //    Serial.print(" | After int: " + String(t37) + "C");
    //    Serial.print(" | Water: " + String(tempWater) + "C");
    //    Serial.println(" | Oil: " + String(tempOil) + "C");

    Serial.println("-------------------------------------------------------------------------------");
    Serial.print("Before int: " + String(pressureBeforeInt) + "bar");
    Serial.print(" | After int: " + String(pressureAfterInt) + "bar");
    Serial.println(" | Oil eng: " + String(pressureOilEngine) + "bar");
    //    Serial.println(" | Oil tur: " + String(pressureOilTurbo) + "bar");
    //    Serial.print(" | Water inject: " + String(pressureWaterInject) + "bar");
    //    Serial.print(" | Manifoil: " + String(pressureManifoil) + "bar");
    //    Serial.println(" | Fuel: " + String(pressureFuel) + "bar");
    Serial.println("-------------------------------------------------------------------------------");
    Serial.println();
  }

  nexLoop(nex_listen_list);

  // delay(DELAY_IN_MILLIS);
}

//---------------------------------------------------------------------------------
// Nextion callbacks
//---------------------------------------------------------------------------------
void bResetMaxPushCallback(void *ptr)
{
  Serial.println("BUTTON PRESS");
  maxValues[0] = 0;
}

//---------------------------------------------------------------------------------
// Helper functions
//---------------------------------------------------------------------------------
float getPressureInBar(int pin, double psiFactor)
{
  int sensorVal = analogRead(pin);

  float voltage = (sensorVal * 5.0) / 1024.0;
  float pressurePascal = (psiFactor * ((float)voltage - 0.469)) * 1000000.0;
  float pressureBar = pressurePascal / 10e5;

  return pressureBar;
}

String getTimestamp()
{
  return String(rtc.getDateStr()) + " " + String(rtc.getTimeStr());
}

//---------------------------------------------------------------------------------
// SD card
//---------------------------------------------------------------------------------

void writeExhaustTemperatures(double ext1, double ext2, double ext3, double ext4, double ext5, double ext6)
{
  fileExhaust = SD.open(FILE_NAME_EXHAUST, FILE_WRITE);

  if (fileExhaust)
  {
    fileExhaust.println(getTimestamp() + "," + String(ext1) + "," + String(ext2) + "," + String(ext3) + "," + String(ext4) + "," + String(ext5) + "," + String(ext6));
    fileExhaust.close();
  }

  //  file = SD.open(FILE_NAME); // open for read;
  //  while (file.available()) {
  //    Serial.write(file.read());
  //  }
}

void writeMiscTemperatures(double water, double oil, double beforeInt, double afterInt)
{
  fileMiscTemp = SD.open(FILE_NAME_MISC_TEMP, FILE_WRITE);

  if (fileMiscTemp)
  {
    fileMiscTemp.println(getTimestamp() + "," + String(water) + "," + String(oil) + "," + String(beforeInt) + "," + String(afterInt));
    fileMiscTemp.close();
  }
}

void writePressure(double beforeInt, double afterInt, double oilEngine, double oilTurbo, double waterInject, double manifoil, double fuel)
{
  filePressure = SD.open(FILE_NAME_PRESSURE, FILE_WRITE);

  if (filePressure)
  {
    filePressure.println(getTimestamp() + "," + String(beforeInt) + "," + String(afterInt) + "," + String(oilEngine) + "," + String(oilTurbo) + "," + String(waterInject) + "," + String(manifoil) + "," + String(fuel));
    filePressure.close();
  }
}

void setupSDCard()
{
  if (!SD.begin(10))
  {
    Serial.println("Initialization of SD card failed!");
    while (1)
      ; // halt program (infinite loop)
  }
  else
  {
    Serial.println("SD card initialization complete.");
  }

  // Create files: open and immediately close.
  fileExhaust = SD.open(FILE_NAME_EXHAUST, FILE_WRITE);
  fileExhaust.println();
  fileExhaust.close();

  fileMiscTemp = SD.open(FILE_NAME_MISC_TEMP, FILE_WRITE);
  fileMiscTemp.println();
  fileMiscTemp.close();

  filePressure = SD.open(FILE_NAME_PRESSURE, FILE_WRITE);
  filePressure.println();
  filePressure.close();
}

//---------------------------------------------------------------------------------
// Date and time
//---------------------------------------------------------------------------------

void setupHardwareTime()
{
  return;

  // Set the clock to run-mode, and disable the write protection
  rtc.halt(false);
  rtc.writeProtect(false);

  rtc.setDOW(WEDNESDAY);
  rtc.setTime(15, 9, 0);
  rtc.setDate(31, 7, 2019);
}