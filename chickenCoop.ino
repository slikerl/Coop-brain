#include "OneWire.h"

#define adcBit 12
#define WATER_PUMP_ON_TEMP 37
#define WATER_PUMP_OFF_TEMP 37
#define WATER_HEAT_ON_TEMP 40
#define WATER_HEAT_OFF_TEMP 40
#define DOOR_STEPS 100

// Define relay pin values
#define waterPumpRelay D3
#define waterHeatRelay D4
#define lightingRelay D5

// define door related pins
#define doorSensor D1
#define doorSwitch D2
#define easyDriverDir A0
#define easyDriverStep A1

// define door direction constants
#define doorDirectionUp TRUE
#define doorDirectionDown FALSE

// define OneWire dignal on pin D0
OneWire ds = OneWire(D0);  // 1-wire signal on pin D0

double tempPublish;
bool doorStatus;
bool doorSwitchCurrentValue;
bool doorSwitchLastValue = FALSE;

// Define door action variable
bool isLastDoorActionOpen = TRUE;
int currentDoorAction = 0; // 0 = stopped, -1 = closing, 1 = opening

void setup() {
  Serial.begin(9600);
  setTempResolution(adcBit);

  // Set relay pin modes
  pinMode(waterPumpRelay, OUTPUT);
  pinMode(waterHeatRelay, OUTPUT);
  pinMode(lightingRelay, OUTPUT);

  // Initialize relay pin values to off
  digitalWrite(waterPumpRelay, FALSE);
  digitalWrite(waterHeatRelay, FALSE);
  digitalWrite(lightingRelay, FALSE);

  // Set door sensor pin mode
  pinMode(doorSensor, INPUT_PULLUP);

  // Set door switch pin mode
  pinMode(doorSwitch, INPUT_PULLUP);

  // Set door motor DIR pin mode
  pinMode(easyDriverDir, OUTPUT);

  // Set door STEP pin mode
  pinMode(easyDriverStep, OUTPUT);

  digitalWrite(easyDriverDir, doorDirectionUp);
  doorStatus = digitalRead(doorSensor);
  while (doorStatus) {
    stepDoorUpOne();
    doorStatus = digitalRead(doorSensor);
  }

  // publish temperature to cloud
  Particle.variable("Temperature", tempPublish);

  // publish door status to cloud
  Particle.variable("DoorClosed", doorStatus);
}

void loop() {
  doorSwitchCurrentValue = digitalRead(doorSwitch);
  // Determine if doorSwitch as been pressed
  if (doorSwitchCurrentValue == TRUE & doorSwitchLastValue == FALSE) {
    // Check to see if door is opening, closing, or stopped
    if (currentDoorAction == 1) {
      // Door was opening, button press causes door to stop
      currentDoorAction = 0;
      isLastDoorActionOpen = TRUE;
    }
    else if (currentDoorAction == -1) {
      currentDoorAction = 0;
      isLastDoorActionOpen = FALSE;
    }
    else if (currentDoorAction == 0 & isLastDoorActionOpen == TRUE) {
      currentDoorAction = -1;
    }
    else if (currentDoorAction == 0 & isLastDoorActionOpen == FALSE) {
      currentDoorAction = 1;
    }

    if (currentDoorAction == -1) {
      Serial.println("closing");
    }
    else if (currentDoorAction == 1) {
      Serial.println("opening");
    }
    else if (currentDoorAction == 0) {
      Serial.println("stopped");
    }
  }

  doorStatus = digitalRead(doorSensor);
  // Check door status
  if (doorStatus) {
    //Serial.println("Door is not open");
  }
  else if (!doorStatus) {
    //Serial.println("Door is 100% open");
  }

  // get current temperature
  float temperature = getTempFahrenheit();
  tempPublish = round((double)temperature*10)/10;
  /*Serial.printf("Temperature: %3.1f F\n\r", temperature);*/

  //transform temperature into integer threshold
  int tempThreshold = (int)temperature;
  /*Serial.printf("Temperature: %d F\n\r", tempThreshold);*/

  // Control circulating water pump
  if (tempThreshold < WATER_PUMP_ON_TEMP) {
    // turn on circulationg water pump
    digitalWrite(waterPumpRelay, TRUE);
  }
  if (tempThreshold > WATER_PUMP_OFF_TEMP) {
    // turn off circulating water pump
    digitalWrite(waterPumpRelay, FALSE);
  }

  // Control water heater
  if (tempThreshold < WATER_HEAT_ON_TEMP) {
    // turn on water heater
    digitalWrite(waterHeatRelay, TRUE);
  }
  if (tempThreshold > WATER_HEAT_OFF_TEMP) {
    // turn off water heater
    digitalWrite(waterHeatRelay, FALSE);
  }

  doorSwitchLastValue = doorSwitchCurrentValue;
}

void setTempResolution(int bitValue) {
  byte configByte;
  byte checkData[5];
  byte addr[8];
  // set configuration byte according to bitValue (9, 10, 11, or 12 bit conversion)
  switch (bitValue) {
    case 9:
      configByte = 0x1F;
      break;
    case 10:
      configByte = 0x3F;
      break;
    case 11:
      configByte = 0x5F;
      break;
    case 12:
      configByte = 0x7F;
      break;
  }

  while ( !ds.search(addr)) {
    ds.reset_search();
  }

  checkData[4] = 0x00;
  if (addr[0] == 0x28)     // check we are really using a DS18B20
  {
    while (configByte != checkData[4]) {
      ds.reset();             // rest 1-Wire
      ds.select(addr);        // select DS18B20

      ds.write(0x4E);         // write on scratchPad
      ds.write(0x00);         // User byte 0 - Unused
      ds.write(0x00);         // User byte 1 - Unused
      ds.write(configByte);   // 9, 10, 11, or 12 bit conversion

      ds.reset();             // reset 1-Wire
      ds.select(addr);
      ds.write(0xBE);
      for ( int i = 0; i < 5; i++) {           // we only need first 4 bytes
        checkData[i] = ds.read();
      }
    }
    ds.reset();
  }
}

float getTempFahrenheit() {
  //returns the temperature from one DS18S20 in DEG Celsius
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  float fahrenheit;

  while ( !ds.search(addr)) {
    ds.reset_search();
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return -9999;
  }

  // this device has temp so let's read it
  ds.reset();               // first clear the 1-wire bus
  ds.select(addr);          // now select the device we just found
  ds.write(0x44, 0);        // tell it to start a conversion, with parasite power on at the end

  byte crcRom = 0x00;
  byte crcCalc = 0x10;

  while (crcRom != crcCalc) {
    // reset, select and read the scratch pad
    present = ds.reset();
    ds.select(addr);
    ds.write(0xBE);

    // read in data
    for ( i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = ds.read();
    }

    // read in ROM crc
    crcRom = data[8];
    // claculate crc check
    crcCalc = OneWire::crc8(data, 8);
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  byte cfg = (data[4] & 0x60);

  // at lower res, the low bits are undefined, so let's zero them
  if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
  if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
  if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  // default is 12 bit resolution, 750 ms conversion time
  fahrenheit = ((float)raw * 0.0625) * 1.8 + 32.0;
  return fahrenheit;
}

void stepDoorUpOne() {
  digitalWrite(easyDriverDir, doorDirectionUp);
  digitalWrite(easyDriverStep, HIGH);
  delay(1);
  digitalWrite(easyDriverStep, LOW);
  delay(1);
}

void stepDoorDownOne() {
  digitalWrite(easyDriverDir, doorDirectionDown);
  digitalWrite(easyDriverStep, HIGH);
  delay(1);
  digitalWrite(easyDriverStep, LOW);
  delay(1);
}
