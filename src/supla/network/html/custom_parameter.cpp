// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR

#include "custom_parameter.h"

template class Supla::Html::CustomParameterTemplate<int32_t>;
template class Supla::Html::CustomParameterTemplate<int16_t>;
template class Supla::Html::CustomParameterTemplate<float>;
template class Supla::Html::CustomParameterTemplate<double>;

#endif
