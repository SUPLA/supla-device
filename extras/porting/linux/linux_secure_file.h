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

// Writes and syncs a temporary file, then atomically renames it into place.
// An existing destination must be a regular file. Returns true if the
// rename succeeds. The parent directory sync is best effort, so the rename
// may be lost after a crash or power loss.
bool writeSecureFileAtomic(const std::string& path,
                           const void* data,
                           std::size_t size);

}  // namespace Supla::Linux

#endif  // EXTRAS_PORTING_LINUX_LINUX_SECURE_FILE_H_
