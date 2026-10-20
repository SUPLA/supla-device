// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_NVSCONFIGTESTS_FAKES_NVS_H_
#define EXTRAS_TEST_NVSCONFIGTESTS_FAKES_NVS_H_

#include <stddef.h>
#include <stdint.h>
#include <cstring>
#include <string>
#include <vector>

using esp_err_t = int;
using nvs_handle_t = uint32_t;
const esp_err_t ESP_OK = 0;
const esp_err_t ESP_ERR_NOT_FOUND = 1;
const esp_err_t ESP_ERR_NVS_NO_FREE_PAGES = 2;
const esp_err_t ESP_ERR_NVS_NOT_FOUND = ESP_ERR_NOT_FOUND;
const esp_err_t ESP_FAIL = -1;
enum nvs_type_t {
  NVS_TYPE_U8, NVS_TYPE_I8, NVS_TYPE_U32, NVS_TYPE_I32, NVS_TYPE_STR,
  NVS_TYPE_BLOB, NVS_TYPE_U64, NVS_TYPE_ANY
};
struct nvs_entry_info_t {
  char namespace_name[16];
  char key[16];
  nvs_type_t type;
};
struct FakeNvsIterator { size_t index; };
using nvs_iterator_t = FakeNvsIterator *;
inline std::vector<nvs_entry_info_t> fakeNvsEntries;
inline FakeNvsIterator fakeNvsIterator = {};
inline bool fakeNvsIteratorActive = false;
inline int fakeNvsIteratorReleases = 0;
inline esp_err_t fakeNvsFindError = ESP_OK;
inline esp_err_t fakeNvsInfoError = ESP_OK;
inline esp_err_t fakeNvsNextError = ESP_OK;
inline std::string fakeNvsEnumeratedPartition;
inline std::string fakeNvsEnumeratedNamespace;
inline esp_err_t nvs_entry_find(const char *partition, const char *ns,
                                nvs_type_t, nvs_iterator_t *it) {
  fakeNvsEnumeratedPartition = partition;
  fakeNvsEnumeratedNamespace = ns;
  *it = nullptr;
  if (fakeNvsFindError != ESP_OK) return fakeNvsFindError;
  fakeNvsIterator.index = 0;
  while (fakeNvsIterator.index < fakeNvsEntries.size() &&
         strcmp(fakeNvsEntries[fakeNvsIterator.index].namespace_name, ns))
    ++fakeNvsIterator.index;
  if (fakeNvsIterator.index == fakeNvsEntries.size())
    return ESP_ERR_NVS_NOT_FOUND;
  *it = &fakeNvsIterator;
  fakeNvsIteratorActive = true;
  return ESP_OK;
}
inline esp_err_t nvs_entry_info(nvs_iterator_t it, nvs_entry_info_t *info) {
  if (fakeNvsInfoError != ESP_OK) return fakeNvsInfoError;
  *info = fakeNvsEntries.at(it->index);
  return ESP_OK;
}
inline esp_err_t nvs_entry_next(nvs_iterator_t *it) {
  if (fakeNvsNextError != ESP_OK) return fakeNvsNextError;
  do {
    ++(*it)->index;
  } while ((*it)->index < fakeNvsEntries.size() &&
           strcmp(fakeNvsEntries[(*it)->index].namespace_name,
                  fakeNvsEnumeratedNamespace.c_str()));
  if ((*it)->index < fakeNvsEntries.size()) return ESP_OK;
  *it = nullptr;
  fakeNvsIteratorActive = false;
  return ESP_ERR_NVS_NOT_FOUND;
}
inline void nvs_release_iterator(nvs_iterator_t) {
  fakeNvsIteratorActive = false;
  ++fakeNvsIteratorReleases;
}
const int NVS_READONLY = 0;
const int NVS_READWRITE = 1;
struct nvs_stats_t {
  size_t used_entries;
  size_t total_entries;
  size_t free_entries;
  size_t namespace_count;
};
extern esp_err_t fakeNvsCommitResult;
extern nvs_handle_t fakeNvsCommitHandle;
extern int fakeNvsCommitCalls;
extern esp_err_t fakeNvsEraseResult;
extern nvs_handle_t fakeNvsEraseHandle;
extern int fakeNvsEraseCalls;
inline esp_err_t nvs_commit(nvs_handle_t handle) {
  ++fakeNvsCommitCalls;
  fakeNvsCommitHandle = handle;
  return fakeNvsCommitResult;
}
inline void nvs_close(nvs_handle_t) {}
inline esp_err_t nvs_get_stats(const char *, nvs_stats_t *stats) {
  *stats = {};
  return ESP_OK;
}
inline esp_err_t nvs_open_from_partition(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_erase_all(nvs_handle_t handle) {
  ++fakeNvsEraseCalls;
  fakeNvsEraseHandle = handle;
  return fakeNvsEraseResult;
}
inline esp_err_t nvs_erase_key(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_get_str(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_get_blob(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_get_i8(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_get_u8(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_get_i32(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_get_u32(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_set_str(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_set_blob(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_set_i8(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_set_u8(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_set_i32(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_set_u32(...) { return ESP_ERR_NOT_FOUND; }

#endif  // EXTRAS_TEST_NVSCONFIGTESTS_FAKES_NVS_H_
