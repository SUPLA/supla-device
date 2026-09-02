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

class File {
 public:
  File() = default;

  explicit operator bool() const { return valid_; }

  bool operator!() const { return !valid_; }

  size_t size() const;
  int read(uint8_t *buffer, size_t size);
  size_t write(const uint8_t *buffer, size_t size);
  void close() { valid_ = false; }
  bool isDirectory() const { return valid_ && directory_; }
  const char *name() const { return name_.c_str(); }
  File openNextFile();

 private:
  friend class FakeLittleFs;

  File(FakeLittleFs *filesystem,
       std::string path,
       bool directory,
       std::string name)
      : filesystem_(filesystem),
        path_(std::move(path)),
        name_(std::move(name)),
        directory_(directory),
        valid_(true) {
  }

  FakeLittleFs *filesystem_ = nullptr;
  std::string path_;
  std::string name_;
  std::string lastDirectoryEntry_;
  size_t position_ = 0;
  bool directory_ = false;
  bool valid_ = false;
};

class FakeLittleFs {
 public:
  bool begin() { return true; }
  void end() {}
  void format() { files_.clear(); }
  void reset() { format(); }

  bool exists(const char *path) const {
    return path != nullptr && files_.find(path) != files_.end();
  }

  bool mkdir(const char *) { return true; }

  bool remove(const char *path) {
    return path != nullptr && files_.erase(path) != 0;
  }

  File open(const char *path, const char *mode) {
    if (path == nullptr || mode == nullptr) {
      return {};
    }

    std::string pathString(path);
    if (pathString == "/supla" && mode[0] == 'r') {
      return File(this, pathString, true, pathString);
    }

    if (mode[0] == 'w') {
      files_[pathString].clear();
      return File(this, pathString, false, pathString);
    }

    if (mode[0] == 'r' && exists(path)) {
      return File(this, pathString, false, pathString);
    }

    return {};
  }

 private:
  friend class File;

  std::map<std::string, std::vector<uint8_t>> files_;
};

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
      std::min(size, data.size() > position_ ? data.size() - position_ : 0);
  std::copy_n(data.begin() + position_, bytesToRead, buffer);
  position_ += bytesToRead;
  return static_cast<int>(bytesToRead);
}

inline size_t File::write(const uint8_t *buffer, size_t size) {
  if (!valid_ || directory_ || buffer == nullptr) {
    return 0;
  }

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
    return File(filesystem_, entry.first, false,
                entry.first.substr(prefix.size()));
  }

  return {};
}

inline FakeLittleFs LittleFS;

#endif  // EXTRAS_TEST_DOUBLES_LITTLEFS_H_
