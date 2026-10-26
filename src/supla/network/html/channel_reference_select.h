// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_CHANNEL_REFERENCE_SELECT_H_
#define SRC_SUPLA_NETWORK_HTML_CHANNEL_REFERENCE_SELECT_H_

#include <inttypes.h>
#include <stdio.h>
#include <supla/channels/channel_reference.h>
#include <supla/network/web_sender.h>

namespace Supla {
namespace Html {

// A token, never an ID supplied by the browser. Keeping always uses the current
// device-side reference, including while its value/peer is unavailable.
inline bool remoteReferenceOption(WebSender *sender, ChannelReference ref) {
  if (ref.kind != ChannelReferenceKind::SERVER_CHANNEL ||
      localReferenceNumber(ref) >= 0) return false;
  char label[80] = {};
  snprintf(label, sizeof(label),
           "Remote channel #%" PRIu32 " - keep assignment",
           ref.id);
  sender->selectOption("keep", label, true);
  return true;
}

// ChannelNumber is 0..254. Reject signs, suffixes, overflow and remote IDs.
inline bool parseLocalChannelNumber(const char *value, int *number) {
  if (!value || !*value) return false;
  unsigned int parsed = 0;
  for (const char *p = value; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    parsed = parsed * 10 + (*p - '0');
    if (parsed > 254) return false;
  }
  *number = parsed;
  return true;
}

}  // namespace Html
}  // namespace Supla
#endif  // SRC_SUPLA_NETWORK_HTML_CHANNEL_REFERENCE_SELECT_H_
