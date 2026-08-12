// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_CUSTOM_TEXT_PARAMETER_H_
#define SRC_SUPLA_NETWORK_HTML_CUSTOM_TEXT_PARAMETER_H_

#include <supla/network/html_element.h>
#include <supla/storage/config.h>
#include <stdint.h>

namespace Supla {

namespace Html {

/**
 * This HTML Element provides input in config mode for text value.
 * You have to provide paramTag under which provided value will be stored
 * in Supla::Storage::Config.
 * paramLabel provides label which is displayed next to input in www.
 */
class CustomTextParameter : public HtmlElement {
 public:
   /**
    * Constructor
    *
    * @param paramTag name that will be used in HTML form and in Supla::Config
    *                to store value. Max length is 16 B (including \0)
    * @param paramLabel label that will be displayed next to input
    * @param maxSize maximum length of value
    */
  CustomTextParameter(
      const char *paramTag, const char *paramLabel, int maxSize);

  /**
   * Destructor
   */
  virtual ~CustomTextParameter();

  /**
   * Sends HTML content
   *
   * @param sender
   */
  void send(Supla::WebSender* sender) override;

  /**
   * Handles HTTP request
   *
   * @param key
   * @param value
   *
   * @return true if element processed requested key, false otherwise
   */
  bool handleResponse(const char* key, const char* value) override;

  /**
   * Returns parameter value
   *
   * @param result buffer to store value
   * @param maxSize size of the buffer
   *
   * @return true if value was found, false otherwise
   */
  bool getParameterValue(char *result, const int maxSize);

  /**
   * Sets parameter value and stores it in Supla::Config
   *
   * @param value value to be stored
   */
  void setParameterValue(const char *value);

 protected:
  char tag[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  char *label = nullptr;
  int maxSize = 0;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_CUSTOM_TEXT_PARAMETER_H_
