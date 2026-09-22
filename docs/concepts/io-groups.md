# IO groups

`Supla::Io::IoGroup` exposes several physical pins as one virtual `IoPin`.
Digital/PWM writes and configuration are forwarded sequentially to every
member. Reads, PWM capabilities and interrupts use the first member. The group
does not create SUPLA channels or save additional state.

```cpp
#include <supla/control/lighting_pwm_leds.h>
#include <supla/io/io_group.h>

// Keep these objects alive for as long as the element uses them.
Supla::Io::IoPin dimmerPins[] = {
    Supla::Io::IoPin(25),
    Supla::Io::IoPin(26),
    Supla::Io::IoPin(27)};
Supla::Io::IoGroup dimmerGroup(dimmerPins, 3);
Supla::Control::LightingPwmLeds dimmer(nullptr, dimmerGroup.getPin());
```

Each member can use its own `Io::Base` backend, for example `IoPin(25, &ledc)`.
For RGB, pass three group handles to the existing `LightingPwmLeds` constructor:
`LightingPwmLeds(nullptr, red.getPin(), green.getPin(), blue.getPin())`.
Their lengths may differ. For RGB+CCT, pass five handles. A single physical
pin can also be passed directly, without a one-member group.

The array is borrowed: the group stores only its pointer and an 8-bit count
(up to 255 members), in addition to the `Io::Base` state. It allocates no memory
and caches no output values. A startup factory loader can fill a persistent
array and then construct the group and element. Keep the array unchanged after
initialization. A null array or zero count produces an unset handle; empty
reads return zero and writes do nothing.

Member entries supply pin numbers, backends and individual polarities. Digital
writes use each member's `writeActive()`/`writeInactive()`; digital reads return
the first member's `readActive()` result. The **outer** `IoPin` from `getPin()`
defaults to active-high. Setting it to active-low inverts the whole group in
addition to each member's polarity. For PWM, the two inversions are combined
using XOR during output configuration; duty values are forwarded unchanged.
All members must support the same PWM range and frequency. There is no scaling,
mixing or validation of hardware capabilities. Analog reads, pulse measurements
and interrupts delegate directly to the first member without polarity mapping.

Set mode and pull-up on the outer `IoPin`; member mode/pull-up flags are ignored.

Use distinct, set pins and avoid cyclic group references. Each group represents
one output; always use its `getPin()` handle (virtual pin 0). `IoPin` equality
does not detect physical overlap between different groups or a group and a raw
pin. In particular, parent/child sharing only recognizes the same group handle.
For a roller shutter, use separate groups for UP and DOWN.

`isReady()` checks all member backends, but does not gate writes. The existing
IO methods return no write status, so the group adds no retry or failure policy.
Fan-out is sequential software IO, not a simultaneous hardware update.
