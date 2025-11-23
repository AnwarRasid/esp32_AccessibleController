// 1. ENABLE NIMBLE MODE
#define USE_NIMBLE 

#include <BleGamepad.h> 
#include <esp_mac.h>

// --- PIN DEFINITIONS (Must use 0/1 for Analog on C3) ---
#define PIN_VRX    0   
#define PIN_VRY    1   
#define PIN_SW     3   

// --- SETTINGS ---
#define DEADZONE 600  

// NAME CHANGE (v10 to force fresh connection)
BleGamepad bleGamepad("BLEJoystick_v10", "BLEJoystick", 100);

// --- CALIBRATION GLOBALS ---
int zeroX = 2048;
int zeroY = 2048;

void setup() {
  Serial.begin(115200);
  
  // Randomize MAC to bypass Windows Cache
  uint8_t newMACAddress[] = {0x32, 0xAE, 0xA4, 0x07, 0x0D, 0xEE}; 
  esp_base_mac_addr_set(newMACAddress);
  
  pinMode(PIN_SW, INPUT_PULLUP);

  // --- AUTO-CALIBRATION ---
  Serial.println("Calibrating... DO NOT TOUCH!");
  delay(1000); 

  long sumX = 0;
  long sumY = 0;
  int samples = 50;
  
  for(int i=0; i<samples; i++) {
    sumX += analogRead(PIN_VRX);
    sumY += analogRead(PIN_VRY);
    delay(10);
  }
  
  zeroX = sumX / samples;
  zeroY = sumY / samples;
  
  // --- HARDWARE HEALTH CHECK ---
  Serial.print("Calibrated Center -> X:"); Serial.print(zeroX);
  Serial.print(" Y:"); Serial.println(zeroY);
  
  if (zeroX < 1500 || zeroY < 1500) {
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("WARNING: VOLTAGE TOO LOW (<1500).");
    Serial.println("Likely Issues:");
    Serial.println("1. Joystick +5V pin is NOT connected to ESP32 3V3.");
    Serial.println("2. Wires are loose/broken.");
    Serial.println("The joystick will NOT work correctly until fixed.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  } else {
    Serial.println("Calibration Looks Good! (Near 2048)");
  }

  // BLE Setup
  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setAutoReport(false); 
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); 
  bleGamepadConfig.setWhichAxes(true, true, false, false, false, false, false, false);
  bleGamepadConfig.setButtonCount(1);
  bleGamepadConfig.setVid(0xe502);
  bleGamepadConfig.setPid(0xabcd);

  bleGamepad.begin(&bleGamepadConfig);
  Serial.println("Bluetooth Advertising...");
}

void loop() {
  int rawX = analogRead(PIN_VRX);
  int rawY = analogRead(PIN_VRY);
  bool btn = !digitalRead(PIN_SW);

  // --- SMART MAPPING (Asymmetric) ---
  int16_t mapX = 0;
  int16_t mapY = 0;

  // Calculate X Axis
  if (rawX > zeroX + DEADZONE) {
    mapX = map(rawX, zeroX + DEADZONE, 4095, 0, 32767);
  } else if (rawX < zeroX - DEADZONE) {
    mapX = map(rawX, 0, zeroX - DEADZONE, -32767, 0);
  }

  // Calculate Y Axis
  if (rawY > zeroY + DEADZONE) {
    mapY = map(rawY, zeroY + DEADZONE, 4095, 0, 32767);
  } else if (rawY < zeroY - DEADZONE) {
    mapY = map(rawY, 0, zeroY - DEADZONE, -32767, 0);
  }

  // Constrain
  mapX = constrain(mapX, -32767, 32767);
  mapY = constrain(mapY, -32767, 32767);

  // --- DIAGNOSTICS ---
  if (millis() % 500 == 0) {
     Serial.print("RawX: "); Serial.print(rawX);
     Serial.print(" | OutX: "); Serial.print(mapX);
     // If raw is ~1000 but OutX is near 0, calibration is hiding the hardware fault
     Serial.println(mapX == 0 ? " (CENTERED)" : " (MOVING)");
  }

  if (bleGamepad.isConnected()) {
    bleGamepad.setAxes(mapX, mapY);
    if (btn) bleGamepad.press(BUTTON_1); 
    else bleGamepad.release(BUTTON_1); 
    bleGamepad.sendReport();
  }
  
  delay(10); 
}