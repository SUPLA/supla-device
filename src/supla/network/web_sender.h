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

#ifndef SRC_SUPLA_NETWORK_WEB_SENDER_H_
#define SRC_SUPLA_NETWORK_WEB_SENDER_H_

#include <supla/network/html_generator.h>

#include <stdint.h>

namespace Supla {

class WebSender;

/**
 * @brief RAII helper for emitting a single HTML tag.
 *
 * The object starts a tag immediately in the constructor:
 * `HtmlTag(sender, "div")` emits `<div`.
 *
 * Attributes can then be appended with chained calls. The tag is closed by:
 * - calling @ref close() to emit `>`
 * - calling @ref body() to emit `>` and a paired closing tag
 * - letting the object go out of scope, which calls @ref end() as a fallback
 *
 * Typical usage:
 * @code
 * sender->tag("div")
 *   .attr("class", "form-field")
 *   .body([&]() {
 *     sender->labelFor("ssid", "SSID");
 *     sender->textInput("ssid", "ssid");
 *   });
 * @endcode
 *
 * For void elements such as `<input>`, use @ref WebSender::voidTag().
 */
class HtmlTag {
 public:
  HtmlTag(WebSender* sender, const char* tagName, bool paired = true);
  ~HtmlTag();

  HtmlTag(const HtmlTag&) = delete;
  HtmlTag& operator=(const HtmlTag&) = delete;

  HtmlTag(HtmlTag&& other) noexcept;
  HtmlTag& operator=(HtmlTag&& other) noexcept;

  /**
   * @brief Append a quoted HTML attribute with escaped value.
   */
  HtmlTag& attr(const char* name, const char* value);

  /**
   * @brief Append a quoted HTML attribute with integer value.
   */
  HtmlTag& attr(const char* name, int value);

  /**
   * @brief Append a boolean HTML attribute when enabled.
   *
   * Example: `checked`, `selected`, `readonly`.
   */
  HtmlTag& attrIf(const char* name, bool enabled);

  /**
   * @brief Emit the closing `>` for the opening tag.
   */
  HtmlTag& close();

  /**
   * @brief Close the opening tag and mark it as finished.
   *
   * For paired tags this prevents the destructor from emitting a closing
   * tag. It is mainly useful for void tags and for low-level control when the
   * caller wants to manage the content separately.
   */
  HtmlTag& finish();

  /**
   * @brief Emit a paired body using a callback.
   *
   * The helper closes the opening tag, executes the callback and then emits
   * the corresponding closing tag.
   */
  template <typename Fn>
  void body(Fn&& fn) {
    close();
    fn();
    end();
  }

  /**
   * @brief Emit a text body using escaped content.
   */
  void body(const char* text);

  /**
   * @brief Explicitly finish the tag.
   *
   * For paired tags this emits the closing tag if needed. Calling this is
   * optional for normal scope-based usage.
   */
  void end();

 private:
  void release();

  WebSender* sender_ = nullptr;
  const char* tagName_ = nullptr;
  bool paired_ = true;
  bool closed_ = false;
  bool finished_ = false;
};

class WebSender {
 public:
  /**
   * @brief Base interface for emitting generated HTML.
   *
   * The class exposes low-level `send()` primitives plus a small streaming
   * HTML builder. The builder is designed for embedded targets and avoids
   * dynamic string assembly.
   *
   * Example:
   * @code
   * sender->formField([&]() {
   *   sender->labelFor("ssid", "Wi-Fi name");
   *   sender->textInput("ssid", "ssid", currentSsid);
   * });
   * @endcode
   *
   * Use the lower-level `tag()` / `voidTag()` helpers when you need direct
   * control over the generated markup.
   */
  virtual ~WebSender();
  virtual void send(const char*, int size = -1) = 0;
  virtual void sendSafe(const char*, int size = -1);
  virtual void send(int number);
  virtual void send(int number, int precision);
  virtual void sendNameAndId(const char *id);
  virtual void sendLabelFor(const char *id, const char *label);
  virtual void sendSelectItem(int value,
                              const char *label,
                              bool selected,
                              bool emptyValue = false);
  virtual void sendHidden(bool hidden);
  virtual void sendReadonly(bool readonly);
  virtual void sendDisabled(bool disabled);
  virtual void sendTimestamp(uint32_t timestamp);

  /**
   * @brief Emit a `<div class="form-field">...</div>` block.
   *
   * The default class name is `form-field`, but it can be overridden when a
   * different wrapper class is needed.
   */
  template <typename Fn>
  void formField(Fn&& fn, const char* className = "form-field") {
    auto field = tag("div");
    field.attr("class", className);
    field.body(fn);
  }

  /**
   * @brief Emit a `<select>` block with optional `name` and `id`.
   */
  template <typename Fn>
  void selectInput(const char* name, const char* id, Fn&& fn) {
    auto select = tag("select");
    if (name) {
      select.attr("name", name);
    }
    if (id) {
      select.attr("id", id);
    }
    select.body(fn);
  }

  /**
   * @brief Emit a `<label for="...">...</label>` pair.
   */
  void labelFor(const char* id, const char* text);

  /**
   * @brief Emit a text input control.
   *
   * Generates an `<input type="text">` element. The `value` attribute is
   * emitted only when `value != nullptr`.
   */
  void textInput(const char* name,
                 const char* id,
                 const char* value = nullptr,
                 int maxLength = -1);

  /**
   * @brief Emit a password input control.
   */
  void passwordInput(const char* name, const char* id);

  /**
   * @brief Emit a checkbox input control.
   *
   * The `value` attribute defaults to `on`, which matches standard HTML
   * checkbox semantics.
   */
  void checkboxInput(const char* name,
                     const char* id,
                     bool checked,
                     const char* value = "on");

  /**
   * @brief Emit a single `<option>` element.
   */
  void selectOption(const char* value,
                    const char* text,
                    bool selected = false);

  /**
   * @brief Start an HTML tag builder.
   *
   * The returned object emits `<tagName` immediately and finishes the tag
   * automatically at scope exit if it has not already been completed.
   */
  HtmlTag tag(const char* tagName, bool paired = true) {
    return HtmlTag(this, tagName, paired);
  }

  /**
   * @brief Start a builder for a void HTML tag such as `<input>`.
   */
  HtmlTag voidTag(const char* tagName) {
    return HtmlTag(this, tagName, false);
  }
};
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_WEB_SENDER_H_
