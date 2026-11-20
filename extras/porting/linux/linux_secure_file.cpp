// SPDX-FileCopyrightText: AC SOFTWARE SP. Z. O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_secure_file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr mode_t kSecureFileMode = S_IRUSR | S_IWUSR;

bool writeAll(int fd, const void* data, std::size_t size) {
  const auto* bytes = static_cast<const char*>(data);
  std::size_t bytesWritten = 0;
  while (bytesWritten < size) {
    const ssize_t result =
        ::write(fd, bytes + bytesWritten, size - bytesWritten);
    if (result > 0) {
      bytesWritten += static_cast<std::size_t>(result);
    } else if (result < 0 && errno == EINTR) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}

std::string parentDirectory(const std::string& path) {
  const auto pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return ".";
  }
  if (pos == 0) {
    return "/";
  }
  return path.substr(0, pos);
}

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

  if (success && !writeAll(fd, data, size)) {
    success = false;
  }

  if (::close(fd) != 0) {
    success = false;
  }

  return success;
}

bool Supla::Linux::writeSecureFileAtomic(const std::string& path,
                                         const void* data,
                                         std::size_t size) {
  if (data == nullptr && size != 0) {
    return false;
  }

  // Refuse to replace anything other than a regular file (e.g. a symlink
  // or a directory) at the destination path.
  struct stat targetStat = {};
  if (::lstat(path.c_str(), &targetStat) == 0) {
    if (!S_ISREG(targetStat.st_mode)) {
      return false;
    }
  } else if (errno != ENOENT) {
    return false;
  }

  // Each write gets its own unique temp file in the destination directory,
  // so concurrent writers never touch each other's data. mkostemp creates it
  // with O_EXCL and mode 0600, so it can't be a planted symlink.
  std::string tmpTemplate = path + ".tmp.XXXXXX";
  const int fd = ::mkostemp(tmpTemplate.data(), O_CLOEXEC);
  if (fd == -1) {
    return false;
  }
  const std::string tmpPath = tmpTemplate;

  bool success = ::fchmod(fd, kSecureFileMode) == 0;
  if (success && !writeAll(fd, data, size)) {
    success = false;
  }
  if (success && ::fsync(fd) != 0) {
    success = false;
  }
  if (::close(fd) != 0) {
    success = false;
  }
  if (success && ::rename(tmpPath.c_str(), path.c_str()) != 0) {
    success = false;
  }
  if (!success) {
    ::unlink(tmpPath.c_str());
    return false;
  }

  // The file is already replaced at this point. Persisting the rename is
  // best effort: reporting failure here would make callers treat new content
  // that is already on disk as not saved.
  const int dirFd = ::open(parentDirectory(path).c_str(),
                           O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (dirFd != -1) {
    ::fsync(dirFd);
    ::close(dirFd);
  }
  return true;
}
