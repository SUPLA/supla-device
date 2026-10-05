// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_NVSCONFIGTESTS_FAKES_NVS_H_
#define EXTRAS_TEST_NVSCONFIGTESTS_FAKES_NVS_H_

#include <stddef.h>
#include <stdint.h>

using esp_err_t = int;
using nvs_handle_t = uint32_t;
const esp_err_t ESP_OK = 0;
const esp_err_t ESP_ERR_NOT_FOUND = 1;
const esp_err_t ESP_ERR_NVS_NO_FREE_PAGES = 2;
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
inline esp_err_t nvs_erase_all(...) { return ESP_ERR_NOT_FOUND; }
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
