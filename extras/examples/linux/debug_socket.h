// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_EXAMPLES_LINUX_DEBUG_SOCKET_H_
#define EXTRAS_EXAMPLES_LINUX_DEBUG_SOCKET_H_

#include <string>

namespace Supla {
class Config;
}  // namespace Supla

bool setupLinuxSupletRuntime(Supla::Config *config);
bool initLinuxDebugSocket(const std::string &path);
void iterateLinuxDebugSocket();

#endif  // EXTRAS_EXAMPLES_LINUX_DEBUG_SOCKET_H_
