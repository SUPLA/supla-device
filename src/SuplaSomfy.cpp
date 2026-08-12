// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef ARDUINO

#include "SuplaSomfy.h"

/*
 Library for Somfy RTS remote controller
 Author: Maciej Królewski
 Using documantation: https://pushstack.wordpress.com/somfy-rts-protocol/
*/

SuplaSomfy::SuplaSomfy(uint8_t dataPin) {
  pinMode(dataPin, OUTPUT);
  digitalWrite(dataPin, LOW);

  _dataPin = dataPin;
}

SuplaSomfy::~SuplaSomfy(void) {
}

void SuplaSomfy::SendBitZero(void) {
  digitalWrite(_dataPin, HIGH);
  delayMicroseconds(BASIC_TIME);
  digitalWrite(_dataPin, LOW);
  delayMicroseconds(BASIC_TIME);
}

void SuplaSomfy::SendBitOne(void) {
  digitalWrite(_dataPin, LOW);
  delayMicroseconds(BASIC_TIME);
  digitalWrite(_dataPin, HIGH);
  delayMicroseconds(BASIC_TIME);
}

uint8_t SuplaSomfy::Checksum(somfy_frame_t *frame) {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < 7; i++) {
    checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
  }

  return checksum & 0xF;
}

void SuplaSomfy::Obfuscation(somfy_frame_t *frame) {
  for (uint8_t i = 1; i < 7; i++) {
    frame[i] ^= frame[i - 1];
  }
}

void SuplaSomfy::SendCommand(somfy_frame_t *frame, uint8_t sync) {
  // Only with the first frame
  if (sync == 2) {
    // Wake-up pulse
    digitalWrite(_dataPin, HIGH);
    delayMicroseconds(9415);
    // Silence pulse
    digitalWrite(_dataPin, LOW);
    delayMicroseconds((unsigned int)89565);
  }

  // Hardware sync (Two sync for the first frame, seven for the next frame)
  for (uint8_t i = 0; i < sync; i++) {
    digitalWrite(_dataPin, HIGH);
    delayMicroseconds(4 * BASIC_TIME);
    digitalWrite(_dataPin, LOW);
    delayMicroseconds(4 * BASIC_TIME);
  }

  // Software sync
  digitalWrite(_dataPin, HIGH);
  delayMicroseconds(4550);
  digitalWrite(_dataPin, LOW);
  delayMicroseconds(BASIC_TIME);

  // Send data bits one by one, starting with MSB
  for (uint8_t i = 0; i < FRAME_SIZE; i++) {
    for (int8_t j = 7; j >= 0; j--) {
      if (bitRead(frame[i], j) == 1) {
        SendBitOne();
      } else {
        SendBitZero();
      }
    }
  }

  digitalWrite(_dataPin, LOW);
  // Between frame silence
  delayMicroseconds(30415);
}

void SuplaSomfy::SetRemote(somfy_remote_t remote) {
  _remote = remote;
}

somfy_remote_t SuplaSomfy::GetRemote() {
  return _remote;
}

void SuplaSomfy::PushButton(ControlButtons pushButton) {
  somfy_frame_t *frame = reinterpret_cast<somfy_frame_t *>(malloc(FRAME_SIZE));

  frame[0] = 0xA7;             // Encryption key
  frame[1] = pushButton << 4;  // MSB - Button pressed, LSB - Checksum
  frame[2] = _remote.rollingCode.svalue.byte1;    // Rolling code (big endian)
  frame[3] = _remote.rollingCode.svalue.byte2;    // Rolling code
  frame[4] = _remote.remoteControl.svalue.byte1;  // Remote address
  frame[5] = _remote.remoteControl.svalue.byte2;  // Remote address
  frame[6] = _remote.remoteControl.svalue.byte3;  // Remote address

#if defined DEBUG_SOMFY
  Serial.print(F("Frame         : "));
  PrintHex8(frame, FRAME_SIZE);
#endif

  // Calculate checksum frame
  frame[1] |= Checksum(frame);

#if defined DEBUG_SOMFY
  Serial.println("");
  Serial.print(F("With checksum : "));
  PrintHex8(frame, FRAME_SIZE);
#endif

  // Obfuscation frame
  Obfuscation(frame);

#if defined DEBUG_SOMFY
  Serial.println("");
  Serial.print(F("Obfuscated    : "));
  PrintHex8(frame, FRAME_SIZE);
#endif

#if defined DEBUG_SOMFY
  Serial.println("");
  Serial.print(F("Rolling Code  : "));
  Serial.println(_remote.rollingCode.ivalue);
  Serial.println("");
#endif

  SendCommand(frame, 2);
  for (uint8_t i = 0; i < 2; i++) {
    SendCommand(frame, 7);
  }

  free(frame);
  _remote.rollingCode.ivalue++;
}

#if defined DEBUG_SOMFY
void SuplaSomfy::PrintHex8(uint8_t *data, uint8_t length) {
  Serial.print("0x");
  for (int i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
}
#endif
#endif /*ARDUINO*/
