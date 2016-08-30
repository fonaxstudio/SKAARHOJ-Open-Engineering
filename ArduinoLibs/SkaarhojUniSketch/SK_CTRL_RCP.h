/**
 * Specific
 */

uint8_t pca9672OutputByte = B11111111; // Bits: 7-0: GPO, GPI, PRV Tally, PGM Tally, Addr4, Addr3, Addr2, Addr1;
int IOchipAddress = B100000;

void addressSwitch_initIO() {
  Wire.beginTransmission(IOchipAddress);
  Wire.write(pca9672OutputByte);
  Wire.endTransmission();
}
uint8_t addressSwitch_getAddress() {  // Returns address of the DIP switch (0-9)
  Wire.requestFrom(IOchipAddress, 1); // asking for two bytes - could ask for more, would just get the same data again.
  return (~Wire.read()) & 0xF;
}
bool addressSwitch_getGPI() {         // Returns 1 if GPI input is shorted (or the GPO output relay is on)
  Wire.requestFrom(IOchipAddress, 1); // asking for two bytes - could ask for more, would just get the same data again.
  return !((Wire.read() >> 6) & 0x1);
}
void addressSwitch_setGPO(bool mode) { // 0=relay off, 1=relay on
  pca9672OutputByte = (pca9672OutputByte & B01111111) | (!mode << 7);

  Wire.beginTransmission(IOchipAddress);
  Wire.write(pca9672OutputByte);
  Wire.endTransmission();
}

/**
 * Hardware setup, config mode and preset settings
 */
void HWcfgDisplay() { Serial << F("SK_MODEL: SK_RCP\n"); }

/**
 * Hardware setup, config mode and preset settings
 */
uint8_t HWsetupL() {
  uint8_t retVal = 0;

  Serial << F("Init BI16 board\n");
  buttons.begin(4);
  buttons.setDefaultColor(0); // Off by default
  buttons.setButtonColorsToDefault();
  if (getConfigMode()) {
    Serial << F("Test: BI16 board color sequence\n");
    buttons.testSequence();
  }
  statusLED(QUICKBLANK);

  // Look for a button press / sustained button press to bring the device into config/config default modes:
  unsigned long timer = millis();
  if (buttons.buttonIsPressedAll() > 0) {
    retVal = 1;
    statusLED(LED_BLUE);
    while (buttons.buttonIsPressedAll() > 0) {
      if (sTools.hasTimedOut(timer, 2000)) {
        retVal = 2;
        statusLED(LED_WHITE);
      }
    }
  } else {
    // Preset
    if (getNumberOfPresets() > 1) {
      uint8_t presetNum = EEPROM.read(EEPROM_PRESET_START + 1);

      while (!sTools.hasTimedOut(timer, 2000) || buttons.buttonIsPressedAll() > 0) {
        uint8_t b16Map[] = {1, 2, 3, 4, 5, 6}; // These numbers refer to the drawing in the web interface
        for (uint8_t a = 0; a < 6; a++) {
          uint8_t color = b16Map[a] <= getNumberOfPresets() ? (b16Map[a] == presetNum ? 1 : 2) : 0;
          buttons.setButtonColor(a + 1, color);
          if (color > 0 && buttons.buttonIsHeldFor(a + 1, 2000)) {
            setCurrentPreset(b16Map[a]);
            buttons.setButtonColor(a + 1, 1);
            while (buttons.buttonIsPressedAll() > 0)
              ;
          }
        }
      }
      buttons.setButtonColorsToDefault();
    }
  };

  // UHB buttons
  Serial << F("Init BI8 board\n");
  buttons2.begin(1);
  buttons2.setDefaultColor(0); // Off by default
  buttons2.setButtonColorsToDefault();
  if (getConfigMode()) {
    Serial << F("Test: BI8 board color sequence\n");
    buttons2.testSequence();
  }
  statusLED(QUICKBLANK);
  if (buttons2.buttonIsPressedAll() > 0) { // Config mode detection:
    retVal = 1;
    statusLED(LED_BLUE);
    while (buttons.buttonIsPressedAll() > 0) {
      if (sTools.hasTimedOut(timer, 2000)) {
        retVal = 2;
        statusLED(LED_WHITE);
      }
    }
  }

  // Encoders
  Serial << F("Init Upper+Left Encoders\n");
  encoders.begin(2);
  // encoders.serialOutput(SK_SERIAL_OUTPUT);
  encoders.setStateCheckDelay(250);
  statusLED(QUICKBLANK);

  Serial << F("Init Lower+Right Encoders\n");
  encoders2.begin(3);
  // encoders2.serialOutput(SK_SERIAL_OUTPUT);
  encoders2.setStateCheckDelay(250);
  statusLED(QUICKBLANK);

  Serial << F("Init ID Display\n");
  idDisplay.begin(1, 0, 1); // DOGM163
  idDisplay.cursor(false);
  idDisplay.print("SKAARHOJ");
  idDisplay.setBacklight(1, 0, 0);
  delay(500);
  idDisplay.setBacklight(0, 1, 0);
  delay(500);
  idDisplay.setBacklight(1, 1, 1);
  statusLED(QUICKBLANK);

  Serial << F("Init Joystick\n");

  uint16_t(&cal1)[3] = getAnalogComponentCalibration(1);
  //  joystick.uniDirectionalSlider_init(10, 35, 35, 0, 1);
  joystick.uniDirectionalSlider_init(cal1[2], cal1[0], cal1[1], 0, 2);
  joystick.uniDirectionalSlider_disableUnidirectionality(true);

  uint16_t(&cal2)[3] = getAnalogComponentCalibration(2);
  wheel.uniDirectionalSlider_init(cal2[2], cal1[0], cal2[1], 0, 3);
  wheel.uniDirectionalSlider_disableUnidirectionality(true);

  joystickbutton.uniDirectionalSlider_init(15, 80, 80, 0, 0);
  joystickbutton.uniDirectionalSlider_disableUnidirectionality(true);

  Serial << F("Init Details Display\n");
  detailsDisplay.begin(4);
  detailsDisplay.clearDisplay(); // clears the screen and buffer
  for (uint8_t a = 0; a < 8; a++) {
    if (a == 4)
      detailsDisplay.setRotation(2);
    detailsDisplay.drawBitmap((a & 3) << 6, 0, welcomeGraphics[a & 3], 64, 32, 1, true);
  }
  detailsDisplay.display(B1);

  // Set Contrast and Current:
  detailsDisplay.sendCommand(0xC1, B1);
  detailsDisplay.sendData(15 << 4, B1);

  // Master Contrast Current Control:
  detailsDisplay.sendCommand(0xC7, B1);
  detailsDisplay.sendData(15, B1);

  addressSwitch_initIO();

  return retVal;
}

/**
 * Hardware test
 */
void HWtestL() {
  static uint16_t testVal = 0;

  encoders.runLoop();
  encoders2.runLoop();

  buttons.testProgramme(0xFFFF);

  encoders.runLoop();
  encoders2.runLoop();

  buttons2.testProgramme(0xF);

  encoders.runLoop();
  encoders2.runLoop();

  idDisplay.gotoRowCol(0, 0);
  idDisplay << _DECPADL(addressSwitch_getAddress(), 2, "0") << (addressSwitch_getGPI() ? F("!") : F(" ")) << _DECPADL(testVal, 5, " ");

  idDisplay.setBacklight(millis() & 0x8000 ? 1 : 0, millis() & 0x4000 ? 1 : 0, millis() & 0x2000 ? 1 : 0);

  addressSwitch_setGPO(millis() & 0x8000);

  if (joystick.uniDirectionalSlider_hasMoved()) {
    testVal = joystick.uniDirectionalSlider_position();
  }

  static uint8_t ptr = 0;

  switch (ptr % 6) {
  case 1:
    testGenerateExtRetVal(ptr + 4);
    writeDisplayTile(detailsDisplay, 0, 0, B1, 3, 1);
    break;
  case 2:
    testGenerateExtRetVal(ptr + 6);
    writeDisplayTile(detailsDisplay, 128, 0, B1, 3);
    break;
  case 3:
    testGenerateExtRetVal(ptr + 7);
    writeDisplayTile(detailsDisplay, 192, 0, B1, 2);
    break;
  case 4:
    testGenerateExtRetVal(ptr + 99);
    writeDisplayTile(detailsDisplay, 0, 32, B1, 1);
    break;
  case 5:
    testGenerateExtRetVal(ptr + 99);
    writeDisplayTile(detailsDisplay, 64, 32, B1, 1);
    break;
  default:
    testGenerateExtRetVal(ptr + 77);
    writeDisplayTile(detailsDisplay, 128, 32, B1, 0, 1);
    break;
  }

  encoders.testProgramme(B11111);
  encoders2.testProgramme(B11111);

  ptr++;
}

/**
 * Hardware runloop
 */
void HWrunLoop() {
  uint16_t bUp;
  uint16_t bDown;
  uint8_t retVal;

  // Address switch:
  static uint8_t currentAddress = 255;
  bDown = addressSwitch_getAddress();

  actionDispatch(53, currentAddress != bDown, false, 0, bDown);
  currentAddress = bDown;

  // BI16 buttons:
  bUp = buttons.buttonUpAll();
  bDown = buttons.buttonDownAll();
  uint8_t b16Map[] = {24, 25, 26, 27, 28, 29, 18, 19, 20, 21, 22, 23, 14, 15, 16, 17}; // These numbers refer to the drawing in the web interface
  for (uint8_t a = 0; a < 16; a++) {
    extRetValPrefersLabel(b16Map[a]);
    uint8_t color = actionDispatch(b16Map[a], bDown & (B1 << a), bUp & (B1 << a));
    buttons.setButtonColor(a + 1, (color & 15) > 0 ? ((!(color & 16) || (millis() & 512) > 0) && ((color & 15) != 5) ? 1 : 3) : 0); // This implements the mono color blink bit
  }
  encoders.runLoop();
  encoders2.runLoop();

  // UHB buttons:
  bUp = buttons2.buttonUpAll();
  bDown = buttons2.buttonDownAll();

  uint8_t b16Map2[] = {47, 50, 49, 48}; // These numbers refer to the drawing in the web interface
  for (uint8_t a = 0; a < 4; a++) {
    extRetValPrefersLabel(b16Map2[a]);
    uint8_t color = actionDispatch(b16Map2[a], bDown & (B1 << a), bUp & (B1 << a));
    buttons2.setButtonColor(a + 1, color & 0xF);
  }
  encoders.runLoop();
  encoders2.runLoop();

  // Joystick:
  bool hasMoved = joystick.uniDirectionalSlider_hasMoved();

  // actionDispatch(41, hasMoved, false, 0, 1000 - joystick.uniDirectionalSlider_position());
  actionDispatch(41, hasMoved, false, 0, constrain(map(joystick.uniDirectionalSlider_position(), 50, 950, 0, 1000), 0, 1000)); // Mapping temporary response to the joystick not being full range. May need redesign...

  // Wheel
  hasMoved = wheel.uniDirectionalSlider_hasMoved();
  actionDispatch(42, hasMoved, false, 0, wheel.uniDirectionalSlider_position());

  // Button
  //  Serial << (joystickbutton.uniDirectionalSlider_position() < 500) << "\n";
  joystickbutton.uniDirectionalSlider_hasMoved();
  static bool lastPosNotPressed = joystickbutton.uniDirectionalSlider_position() < 500;
  actionDispatch(43, lastPosNotPressed && (joystickbutton.uniDirectionalSlider_position() > 500), !lastPosNotPressed && (joystickbutton.uniDirectionalSlider_position() < 500));
  lastPosNotPressed = joystickbutton.uniDirectionalSlider_position() < 500;

  // Encoders
  uint8_t encMap[] = {30, 31, 32, 33, 39}; // These numbers refer to the drawing in the web interface
  HWrunLoop_encoders(encoders, encMap, sizeof(encMap), true);

  // Encoders2
  uint8_t encMap2[] = {34, 35, 36, 37, 40}; // These numbers refer to the drawing in the web interface
  HWrunLoop_encoders(encoders2, encMap2, sizeof(encMap2), true);

  encoders.runLoop();
  encoders2.runLoop();

  // Details display, 256x64 OLED:
  static uint16_t detailsDisplay_prevHash[13];
  static bool detailsDisplay_written[5];
  bool detailsDisplay_update = false;
  for (uint8_t a = 0; a < 13; a++) {
    extRetValIsWanted(true);
    retVal = actionDispatch(a + 1);
    if (a < 5)
      detailsDisplay_written[a] = false;

    if (detailsDisplay_prevHash[a] != extRetValHash()) {
      detailsDisplay_prevHash[a] = extRetValHash();
      detailsDisplay_update = true;

      if (a == 0) { // Main display
        detailsDisplay_written[a] = retVal != 0;
        detailsDisplay.zoom2x(true);
        writeDisplayTile(detailsDisplay, 0, 0, B0, 0, 2);
        if (!retVal) { // In case blanking, make sure other display layers are evaluated:
          for (uint8_t b = 1; b < 13; b++) {
            detailsDisplay_prevHash[b]++;
          }
        }
      } else if (a >= 1 && a <= 4) { // Q1-Q4
        detailsDisplay_written[a] = retVal != 0;
        if (!detailsDisplay_written[0] || retVal != 0) { // Write if a) 256x64 was not written with non-blank content and b) if this display has non-blank content
          detailsDisplay.zoom2x(false);
          writeDisplayTile(detailsDisplay, ((a - 1) & 1) << 7, ((a - 1) & 2) << 4, B0, ~(a - 1), 1);
          if (!retVal) { // In case blanking, make sure other display layers are evaluated:
            for (uint8_t b = 5; b < 13; b++) {
              detailsDisplay_prevHash[b]++;
            }
          }
        }
      } else {
        if (!(detailsDisplay_written[0] || detailsDisplay_written[((a - 5) >> 1) + 1]) || retVal != 0) { // Write if a) previous displays was not written with non-blank content and b) if this display has non-blank content
          detailsDisplay.zoom2x(false);
          writeDisplayTile(detailsDisplay, ((a - 5) & 3) << 6, ((a - 5) & 4) << 3, B0, (((a - 5) & 3) != 3) | ((a - 5) & 4 ? 0 : 2), 0);
        }
      }
      Serial << "Write! " << a << "\n";
    }
  }
  if (detailsDisplay_update)
    detailsDisplay.display(B1);

  encoders.runLoop();
  encoders2.runLoop();

#if (SK_HWEN_STDOLEDDISPLAY)
  static uint16_t infoDisplay_prevHash[3];
  static bool infoDisplay_written;
  HWrunLoop_128x32OLED(infoDisplay, 44, infoDisplay_prevHash, infoDisplay_written);
#endif

  // ID Display:
  extRetValIsWanted(true);
  retVal = (actionDispatch(38) & 0xF);
  bool idDisplay_clrs[6][3] = {{0, 0, 0}, {1, 1, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 1}, {1, 0, 1}};
  static uint8_t idDisplay_prevVal = 0;
  if (idDisplay_prevVal != retVal) {
    idDisplay_prevVal = retVal;
    idDisplay.setBacklight(idDisplay_clrs[retVal][0], idDisplay_clrs[retVal][1], idDisplay_clrs[retVal][2]);
    Serial << "Set backlight!";
  }

  static uint16_t idDisplay_prevHash = 0;
  if (idDisplay_prevHash != extRetValHash()) {
    idDisplay_prevHash = extRetValHash();
    idDisplay.clearDisplay();
    idDisplay.gotoRowCol(0, 0);
    idDisplay << _extRetTxt[0];
    Serial << "Write!\n";
  }

  // GPI
  bUp = bDown = 0;
  static bool gpiCache = false;
  if (addressSwitch_getGPI()) {
    if (!gpiCache) {
      gpiCache = true;
      bDown = 1;
    }
  } else {
    if (gpiCache) {
      gpiCache = false;
      bUp = 1;
    }
  }

  actionDispatch(52, bDown, bUp);

  // GPO:
  static bool gpoCache = false;
  uint8_t state = actionDispatch(51) & 0xF;
  if (state != 5 && state != 0) {
    if (!gpoCache) {
      gpoCache = true;
      addressSwitch_setGPO(true);
    }
  } else {
    if (gpoCache) {
      gpoCache = false;
      addressSwitch_setGPO(false);
    }
  }
}

uint8_t HWnumOfAnalogComponents() { return 2; }

int16_t HWAnalogComponentValue(uint8_t num) {
  switch (num) {
  case 1:
    return joystick.uniDirectionalSlider_rawValue();
    break;
  case 2:
    return wheel.uniDirectionalSlider_rawValue();
    break;
  }
}
