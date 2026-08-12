// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLASOMFY_H_
#define SRC_SUPLASOMFY_H_

#include "Arduino.h"

#define DEBUG_SOMFY

// ----- DO NOT CHANGE -----
#define FRAME_SIZE 7    // Somfy RTS frame size
#define BASIC_TIME 604  // Basic time [us]
// -------------------------

typedef uint8_t somfy_frame_t;
typedef union {
  struct {
    uint8_t byte1;
    uint8_t byte2;
  } svalue;
  uint8_t tvalue[2];
  uint16_t ivalue;
} somfy_rollingcode_t;
typedef union {
  struct {
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
  } svalue;
  uint8_t tvalue[3];
  uint32_t ivalue : 24;
} somfy_remotesn_t;

enum /*class*/ ControlButtons {
  STOP = 0x1,  // [My]          Stop or move to favourite position
  UP = 0x2,    // [Up]          Move Up
  MYUP = 0x3,  // [My + Up]     Set upper motor limit in initial
               // programming mode
  DOWN = 0x4,  // [Down]        Move Down
  MYDOWN =
      0x5,  // [My + Down]   Set lower motor limit in initial programming mode
  UPDOWN = 0x6,  // [Up + Down]   Change motor limit and initial
                 // programming mode
  PROG = 0x8,    // [Prog]        Registering / Deregistering remotes
  SUN = 0x9,  // [Sun + Flag]  Enable sun and wind detector (SUN and FLAG
              // TIME on the Telis Soliris RC)
  FLAG = 0xA  // [Flag]        Disable sun detector (FLAG TIME on the Telis
              // Soliris RC)
};

struct somfy_remote_t {
  somfy_rollingcode_t rollingCode;
  somfy_remotesn_t remoteControl;
};

class SuplaSomfy {
 private:
  uint8_t _dataPin;
  somfy_remote_t _remote;

  void SendBitZero(void);
  void SendBitOne(void);
  uint8_t Checksum(somfy_frame_t *frame);
  void Obfuscation(somfy_frame_t *frame);
  void SendCommand(somfy_frame_t *frame, uint8_t sync);

 public:
  explicit SuplaSomfy(uint8_t dataPin);
  ~SuplaSomfy(void);
  void SetRemote(somfy_remote_t remote);
  somfy_remote_t GetRemote(void);
  void PushButton(ControlButtons pushButton);

#if defined DEBUG_SOMFY
  void PrintHex8(uint8_t *data, uint8_t length);
#endif
};

#endif  // SRC_SUPLASOMFY_H_
