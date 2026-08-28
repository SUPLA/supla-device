// SPDX-FileCopyrightText: AC SOFTWARE SP. Z. O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_secure_file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <string>

namespace {

constexpr mode_t kSecureFileMode = S_IRUSR | S_IWUSR;

}  // namespace

bool Supla::Linux::writeSecureFile(const std::string& path,
                                   const void* data,
                                   std::size_t size,
                                   bool append) {
  int flags = O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW;
  if (append) {
    flags |= O_APPEND;
  }

  const int fd = ::open(path.c_str(), flags, kSecureFileMode);
  if (fd == -1) {
    return false;
  }

  bool success = data != nullptr || size == 0;
  struct stat fileStat = {};
  if (success &&
      (::fstat(fd, &fileStat) != 0 || !S_ISREG(fileStat.st_mode))) {
    success = false;
  }
  if (success && ::fchmod(fd, kSecureFileMode) != 0) {
    success = false;
  }
  if (success && !append && ::ftruncate(fd, 0) != 0) {
    success = false;
  }

  const auto* bytes = static_cast<const char*>(data);
  std::size_t bytesWritten = 0;
  while (success && bytesWritten < size) {
    const ssize_t result =
        ::write(fd, bytes + bytesWritten, size - bytesWritten);
    if (result > 0) {
      bytesWritten += static_cast<std::size_t>(result);
    } else if (result < 0 && errno == EINTR) {
      continue;
    } else {
      success = false;
    }
  }

  if (::close(fd) != 0) {
    success = false;
  }

  return success;
}
