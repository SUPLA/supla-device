/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef SRC_SUPLA_STORAGE_KEY_VALUE_H_
#define SRC_SUPLA_STORAGE_KEY_VALUE_H_

#include <stddef.h>
#include <stdint.h>

#include "config.h"

#define SUPLA_STORAGE_KEY_SIZE 15

namespace Supla {
class KeyValueElement;

class KeyValue : public Config {
 public:
  ~KeyValue();
  bool initFromMemory(uint8_t* input, size_t inputSize);
  // returns size of written structure
  size_t serializeToMemory(uint8_t* output, size_t outputMaxSize);
  void removeAllMemory();

  bool generateGuidAndAuthkey() override;

  void removeAll() override;
  bool setString(const char* key, const char* value) override;
  bool getString(const char* key, char* value, size_t maxSize) override;
  int getStringSize(const char* key) override;

  bool setBlob(const char* key, const char* value, size_t blobSize) override;
  bool getBlob(const char* key, char* value, size_t blobSize) override;

  bool getInt8(const char* key, int8_t* result) override;
  bool getUInt8(const char* key, uint8_t* result) override;
  bool getInt32(const char* key, int32_t* result) override;
  bool getUInt32(const char* key, uint32_t* result) override;

  bool setInt8(const char* key, const int8_t value) override;
  bool setUInt8(const char* key, const uint8_t value) override;
  bool setInt32(const char* key, const int32_t value) override;
  bool setUInt32(const char* key, const uint32_t value) override;
  bool eraseKey(const char* key) override;

 protected:
  int getBlobSize(const char* key) override;
  KeyValueElement* find(const char* key);
  KeyValueElement* findOrCreate(const char* key);
  KeyValueElement* first = nullptr;
};

enum DataType {
  DATA_TYPE_NOT_SET = 0,
  DATA_TYPE_UINT8,
  DATA_TYPE_INT8,
  DATA_TYPE_UINT32,
  DATA_TYPE_INT32,
  DATA_TYPE_BLOB,
  DATA_TYPE_STRING
};

class KeyValueElement {
 public:
  explicit KeyValueElement(const char* keyName);
  ~KeyValueElement();
  bool isKeyEqual(const char* keyToCheck);
  KeyValueElement* getNext();
  bool hasNext();
  void setNext(KeyValueElement* toBeSet);
  void add(KeyValueElement* toBeAdded);

  size_t serialize(uint8_t* destination, size_t maxSize);

  bool setString(const char* value);
  bool getString(char* value, size_t maxSize);
  int getStringSize();

  bool setBlob(const char* value, size_t blobSize);
  bool getBlob(char* value, size_t blobSize);
  int getBlobSize();

  bool getInt8(int8_t* result);
  bool getUInt8(uint8_t* result);
  bool getInt32(int32_t* result);
  bool getUInt32(uint32_t* result);

  bool setInt8(const int8_t value);
  bool setUInt8(const uint8_t value);
  bool setInt32(const int32_t value);
  bool setUInt32(const uint32_t value);

 protected:
  KeyValueElement* next = nullptr;
  char key[SUPLA_STORAGE_KEY_SIZE] = {};
  enum DataType dataType = DATA_TYPE_NOT_SET;
  unsigned int size = 0;  // set only for blob and string
  union {
    uint8_t* uint8ptr = nullptr;
    char* charPtr;
    uint8_t uint8;
    int8_t int8;
    uint32_t uint32;
    int32_t int32;
  } data;
};
};  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_KEY_VALUE_H_
