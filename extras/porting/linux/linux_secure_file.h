// SPDX-FileCopyrightText: AC SOFTWARE SP. Z. O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_SECURE_FILE_H_
#define EXTRAS_PORTING_LINUX_LINUX_SECURE_FILE_H_

#include <cstddef>
#include <string>

namespace Supla::Linux {

bool writeSecureFile(const std::string& path,
                     const void* data,
                     std::size_t size,
                     bool append);

}  // namespace Supla::Linux

#endif  // EXTRAS_PORTING_LINUX_LINUX_SECURE_FILE_H_
