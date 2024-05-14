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

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CONTROL_TEMPLATE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CONTROL_TEMPLATE_H_

#include <supla/template/template.h>

#include <string>
#include <variant>
#include <map>

namespace Supla {
namespace Template {

class ControlTemplateBase {
 public:
  explicit ControlTemplateBase(Supla::Template::Template*);

  void setMapping(const std::string &parameter, const std::string &key);
  void setMapping(const std::string &parameter, const int index);

  void setSetOnValue(const std::variant<int, bool, std::string>& value);
  void setSetOffValue(const std::variant<int, bool, std::string>& value);

 protected:
  // template configuration
  int id;
  Supla::Template::Template* templateValue = nullptr;
  std::map<std::string, std::string> parameter2Key;
  std::variant<int, bool, std::string> setOnValue;
  std::variant<int, bool, std::string> setOffValue;
};

template <typename T>
class ControlTemplate : public T, public ControlTemplateBase {
 public:
  explicit ControlTemplate(Supla::Template::Template *);
};

template <typename T>
ControlTemplate<T>::ControlTemplate(Supla::Template::Template *templateValue)
    : ControlTemplateBase(templateValue) {
}

}  // namespace Template
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CONTROL_TEMPLATE_H_
