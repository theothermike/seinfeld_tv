//-------------------------------------------------------------------------------
//  SeinfeldTV - IR Remote Input (modified from upstream)
//
//  Split input into raw events (RawInputFlags) so the state machine can
//  remap encoder/IR to different actions depending on state.
//  Also still sets upstream inputFlags for backward compatibility with
//  SD_AVI/display code during PLAYBACK state.
//-------------------------------------------------------------------------------

volatile struct TinyIRReceiverCallbackDataStruct sCallbackData;

void initializeInfrared() {
  if (!initPCIInterruptForTinyReceiver()) {
    dbgPrint("No interrupt available for IR_INPUT_PIN");
  }
}

void handleReceivedTinyIRData(uint16_t aAddress, uint8_t aCommand, bool isRepeat) {
  sCallbackData.Address = aAddress;
  sCallbackData.Command = aCommand;
  sCallbackData.isRepeat = isRepeat;
  sCallbackData.justWritten = true;
}

// Set raw input flags from IR (state-independent)
void IRInputRaw(RawInputFlags* raw) {
  if (sCallbackData.justWritten) {
    sCallbackData.justWritten = false;
    if (sCallbackData.Command == 0x1)
      raw->irChannelUp = true;
    if (sCallbackData.Command == 0x4)
      raw->irChannelDn = true;
    if (sCallbackData.Command == 0xF)
      raw->irMute = true;
    if (sCallbackData.Command == 0xD)
      raw->irVolDn = true;
    if (sCallbackData.Command == 0xE)
      raw->irVolUp = true;
    if (sCallbackData.Command == 0x11)
      raw->irPower = true;
  }
}

// Upstream-compatible IR handler (used during PLAYBACK state)
void IRInput(inputFlagStruct* inputFlags) {
  if (sCallbackData.justWritten) {
    sCallbackData.justWritten = false;
    if (sCallbackData.Command == 0x1)
      inputFlags->channelUp = true;
    if (sCallbackData.Command == 0x4)
      inputFlags->channelDown = true;
    if (sCallbackData.Command == 0xF)
      inputFlags->mute = true;
    if (sCallbackData.Command == 0xD)
      inputFlags->volDown = true;
    if (sCallbackData.Command == 0xE)
      inputFlags->volUp = true;
    if (sCallbackData.Command == 0x11)
      inputFlags->power = true;
  }
}

// Read raw encoder events into RawInputFlags (for browser states)
void updateRawButtonStates(RawInputFlags* raw) {
  if (encoderDirection > 0) {
    encoderDirection = 0;
    raw->encoderCW = true;
  }
  if (encoderDirection < 0) {
    encoderDirection = 0;
    raw->encoderCCW = true;
  }
  if (encoder2Direction > 0) {
    encoder2Direction = 0;
    raw->encoder2CW = true;
  }
  if (encoder2Direction < 0) {
    encoder2Direction = 0;
    raw->encoder2CCW = true;
  }
  if (powerButtonWasPressed && (millis() - powerButtonWasPressed > 20)) {
    if (digitalRead(POWER_BTN_PIN) == LOW) {
      powerButtonWasPressed = 0;
      raw->power = true;
    }
  }
}
