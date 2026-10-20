// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_LITTLEFS_H_
#define EXTRAS_TEST_DOUBLES_LITTLEFS_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

class FakeLittleFs;
using lfs_t = FakeLittleFs;
struct lfs_dir_t {
  std::string lastPath;
  bool opened = false;
};
struct lfs_info {
  uint8_t type;
  char name[64];
};
constexpr int LFS_ERR_NOENT = -2;
constexpr int LFS_ERR_IO = -5;
constexpr uint8_t LFS_TYPE_REG = 1;
constexpr uint8_t LFS_TYPE_DIR = 2;

struct LittleFSConfig {
  explicit LittleFSConfig(bool autoFormat = true) : autoFormat(autoFormat) {}
  bool autoFormat;
};

class File {
 public:
  File() = default;

  explicit operator bool() const { return valid_; }

  bool operator!() const { return !valid_; }

  size_t size() const;
  int read(uint8_t *buffer, size_t size);
  size_t write(const uint8_t *buffer, size_t size);
  void close();
  bool isDirectory() const { return valid_ && directory_; }
  const char *name() const { return name_.c_str(); }
  File openNextFile();

 private:
  friend class FakeLittleFs;

  File(FakeLittleFs *filesystem,
       std::string path,
       bool directory,
       std::string name,
       bool writable = false)
      : filesystem_(filesystem),
        path_(std::move(path)),
        name_(std::move(name)),
        directory_(directory),
        valid_(true),
        writable_(writable) {
  }

  FakeLittleFs *filesystem_ = nullptr;
  std::string path_;
  std::string name_;
  std::string lastDirectoryEntry_;
  size_t position_ = 0;
  bool directory_ = false;
  bool valid_ = false;
  bool writable_ = false;
};

class FakeLittleFs {
 public:
  bool setConfig(const LittleFSConfig &config) {
    autoFormat_ = config.autoFormat;
    return true;
  }
  bool begin() {
    bool result = ++beginCalls != failBeginCall && beginResult;
    if (!result && autoFormat_) {
      format();
    }
    return result;
  }
  void end() {
    // Failed close/sync can leave readable cached bytes until unmount.
    for (const auto &entry : lostWrites_) {
      files_[entry.first] = entry.second;
    }
    lostWrites_.clear();
  }
  void format() {
    files_.clear();
    lostWrites_.clear();
  }
  void reset() {
    format();
    beginResult = true;
    autoFormat_ = true;
    failOpenPath.clear();
    failReadPath.clear();
    failRemovePath.clear();
    failClosePath.clear();
    failSyncPath.clear();
    corruptClosePath.clear();
    beginCalls = 0;
    failBeginCall = -1;
    failDirectoryReadCall = -1;
    directoryReadCalls = 0;
    openDirectories = 0;
    failDirectoryClose = false;
    directoryMissing = false;
    maxWrite = SIZE_MAX;
    maxRead = SIZE_MAX;
  }

  int failDirectoryReadCall = -1;
  int directoryReadCalls = 0;
  int openDirectories = 0;
  bool failDirectoryClose = false;
  bool directoryMissing = false;
  lfs_t *native() { return this; }
  bool beginResult = true;
  std::string failOpenPath;
  std::string failReadPath;
  std::string failRemovePath;
  std::string failClosePath;
  std::string failSyncPath;
  std::string corruptClosePath;
  int beginCalls = 0;
  int failBeginCall = -1;
  size_t maxWrite = SIZE_MAX;
  size_t maxRead = SIZE_MAX;

  bool exists(const char *path) const {
    // The directory is always exposed by open() in this fake.
    return path != nullptr &&
        (std::string(path) == "/supla" || files_.find(path) != files_.end());
  }

  bool mkdir(const char *) { return true; }

  bool remove(const char *path) {
    return path != nullptr && path != failRemovePath &&
        files_.erase(path) != 0;
  }

  File open(const char *path, const char *mode) {
    if (path == nullptr || mode == nullptr || path == failOpenPath) {
      return {};
    }

    std::string pathString(path);
    if (pathString == "/supla" && mode[0] == 'r') {
      return File(this, pathString, true, pathString);
    }

    if (mode[0] == 'w') {
      if (pathString == failClosePath || pathString == failSyncPath) {
        lostWrites_[pathString] = files_[pathString];
      }
      files_[pathString].clear();
      return File(this, pathString, false, pathString, true);
    }

    if (mode[0] == 'r' && pathString != failReadPath && exists(path)) {
      return File(this, pathString, false, pathString);
    }

    return {};
  }

 private:
  friend class File;
  friend int lfs_dir_read(lfs_t *, lfs_dir_t *, lfs_info *);

  bool autoFormat_ = true;
  std::map<std::string, std::vector<uint8_t>> files_;
  std::map<std::string, std::vector<uint8_t>> lostWrites_;
};

inline void File::close() {
  if (valid_ && writable_ && path_ == filesystem_->corruptClosePath) {
    auto &data = filesystem_->files_[path_];
    if (!data.empty()) {
      data.back() ^= 1;
    }
  }
  valid_ = false;
}

inline size_t File::size() const {
  if (!valid_ || directory_) {
    return 0;
  }
  return filesystem_->files_.at(path_).size();
}

inline int File::read(uint8_t *buffer, size_t size) {
  if (!valid_ || directory_ || buffer == nullptr) {
    return -1;
  }

  const auto &data = filesystem_->files_.at(path_);
  const size_t bytesToRead =
      std::min(std::min(size, filesystem_->maxRead),
               data.size() > position_ ? data.size() - position_ : 0);
  std::copy_n(data.begin() + position_, bytesToRead, buffer);
  position_ += bytesToRead;
  return static_cast<int>(bytesToRead);
}

inline size_t File::write(const uint8_t *buffer, size_t size) {
  if (!valid_ || directory_ || buffer == nullptr) {
    return 0;
  }

  size = std::min(size, filesystem_->maxWrite);
  auto &data = filesystem_->files_.at(path_);
  if (data.size() < position_ + size) {
    data.resize(position_ + size);
  }
  std::copy_n(buffer, size, data.begin() + position_);
  position_ += size;
  return size;
}

inline File File::openNextFile() {
  if (!valid_ || !directory_) {
    return {};
  }

  const std::string prefix = path_ + "/";
  for (const auto &entry : filesystem_->files_) {
    if (entry.first.rfind(prefix, 0) != 0 ||
        entry.first.find('/', prefix.size()) != std::string::npos ||
        entry.first <= lastDirectoryEntry_) {
      continue;
    }
    lastDirectoryEntry_ = entry.first;
    // Match the real Arduino iterator: a failed entry open also returns File().
    auto file = filesystem_->open(entry.first.c_str(), "r");
    if (file) file.name_ = entry.first.substr(prefix.size());
    return file;
  }

  return {};
}

inline int lfs_dir_open(lfs_t *fs, lfs_dir_t *dir, const char *path) {
  if (fs->directoryMissing) return LFS_ERR_NOENT;
  if (fs->failOpenPath == path) return LFS_ERR_IO;
  dir->lastPath.clear();
  dir->opened = true;
  ++fs->openDirectories;
  return 0;
}
inline int lfs_dir_read(lfs_t *fs, lfs_dir_t *dir, lfs_info *info) {
  if (!dir->opened || ++fs->directoryReadCalls == fs->failDirectoryReadCall)
    return LFS_ERR_IO;
  for (const auto &entry : fs->files_) {
    const std::string prefix = "/supla/";
    if (entry.first.rfind(prefix, 0) != 0 ||
        entry.first.find('/', prefix.size()) != std::string::npos ||
        entry.first <= dir->lastPath) continue;
    dir->lastPath = entry.first;
    const auto name = entry.first.substr(prefix.size());
    if (name.size() >= sizeof(info->name)) return LFS_ERR_IO;
    std::copy(name.begin(), name.end(), info->name);
    info->name[name.size()] = 0;
    info->type = LFS_TYPE_REG;
    return 1;
  }
  return 0;
}
inline int lfs_dir_close(lfs_t *fs, lfs_dir_t *dir) {
  if (dir->opened) --fs->openDirectories;
  dir->opened = false;
  return fs->failDirectoryClose ? LFS_ERR_IO : 0;
}

inline FakeLittleFs LittleFS;

#endif  // EXTRAS_TEST_DOUBLES_LITTLEFS_H_
