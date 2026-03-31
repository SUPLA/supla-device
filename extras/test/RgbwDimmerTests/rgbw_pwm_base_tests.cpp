/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <gtest/gtest.h>

#include <supla/control/rgbw_pwm_base.h>
#include <supla/io.h>

class IoStub : public Supla::Io::Base {
 public:
  IoStub() : Base(false) {}
};

class RgbwPwmBaseForTest : public Supla::Control::RGBWPwmBase {
 public:
  RgbwPwmBaseForTest(int out1,
                     int out2,
                     int out3,
                     int out4,
                     int out5)
      : RGBWPwmBase(nullptr, out1, out2, out3, out4, out5) {}

 protected:
  void initializePwmTimer() override {}
  int initializePwmChannel(int outputIndex) override { return outputIndex; }
  void writePwmValue(int channel, uint32_t value) override {
    (void)(channel);
    (void)(value);
  }
  void stopPwmChannel(int channel) override { (void)(channel); }
};

TEST(RgbwPwmBaseTests, StoresPerOutputIoSeparately) {
  RgbwPwmBaseForTest pwm(11, 12, 13, 14, 15);
  IoStub io1;
  IoStub io2;

  EXPECT_EQ(pwm.getOutputPin(0), 11);
  EXPECT_EQ(pwm.getOutputPin(4), 15);
  EXPECT_EQ(pwm.getOutputIo(0), nullptr);
  EXPECT_EQ(pwm.getOutputIo(4), nullptr);

  pwm.setOutputIo(0, &io1);
  pwm.setOutputIo(3, &io2);

  EXPECT_EQ(pwm.getOutputIo(0), &io1);
  EXPECT_EQ(pwm.getOutputIo(1), nullptr);
  EXPECT_EQ(pwm.getOutputIo(3), &io2);
  EXPECT_EQ(pwm.getOutputIo(4), nullptr);
}
