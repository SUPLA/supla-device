/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include "control_template.h"

Supla::Template::ControlTemplateBase::ControlTemplateBase(
    Supla::Template::Template* templateValue)
    : templateValue(templateValue) {
  static int instanceCounter = 0;
  id = instanceCounter++;
}

void Supla::Template::ControlTemplateBase::setMapping(
    const std::string& parameter, const std::string& key) {
  parameter2Key[parameter] = key;
  templateValue->addKey(key, -1);  // ignore index
}

void Supla::Template::ControlTemplateBase::setMapping(
    const std::string& parameter, const int index) {
  std::string key = parameter;
  key += "_";
  key += std::to_string(id);
  parameter2Key[parameter] = key;
  templateValue->addKey(key, index);
}

void Supla::Template::ControlTemplateBase::setSetOnValue(
    const std::variant<int, bool, std::string>& value) {
  setOnValue = value;
}

void Supla::Template::ControlTemplateBase::setSetOffValue(
    const std::variant<int, bool, std::string>& value) {
  setOffValue = value;
}
