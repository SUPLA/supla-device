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

#ifndef EXTRAS_TEST_DOUBLES_ELEMENT_WITH_STORAGE_H_
#define EXTRAS_TEST_DOUBLES_ELEMENT_WITH_STORAGE_H_

#include <supla/element.h>

class ElementWithStorage : public Supla::Element {
 public:
  ElementWithStorage();
  void onLoadState();
  void onSaveState();

  int32_t stateValue = -1;
};

#endif  // EXTRAS_TEST_DOUBLES_ELEMENT_WITH_STORAGE_H_
