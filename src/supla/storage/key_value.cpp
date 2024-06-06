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

#include "key_value.h"

#include <string.h>
#include <supla-common/proto.h>
#include <supla/log_wrapper.h>

#if defined(ESP8266)
#include <Esp.h>
#elif defined(ESP32)
#include <esp_random.h>
#endif

namespace Supla {
KeyValue::~KeyValue() {
  removeAllMemory();
}

void KeyValue::removeAllMemory() {
  auto element = first;
  while (element) {
    first = element->getNext();
    delete element;
    element = first;
  }
}

void KeyValue::removeAll() {
  removeAllMemory();
  commit();
}

bool KeyValue::generateGuidAndAuthkey() {
  uint8_t guid[SUPLA_GUID_SIZE] = {};
  uint8_t authkey[SUPLA_AUTHKEY_SIZE] = {};

  // Both ESP varaints use HW RNG after enabling Wi-Fi, so there is no need
  // to initialize seed
#if defined(ESP8266)
  ESP.random(guid, SUPLA_GUID_SIZE);
  ESP.random(authkey, SUPLA_AUTHKEY_SIZE);
#elif defined(ESP32)
  esp_fill_random(guid, SUPLA_GUID_SIZE);
  esp_fill_random(authkey, SUPLA_AUTHKEY_SIZE);
#else
  // TODO(klew): Arduino MEGA
#endif

  setGUID(reinterpret_cast<char*>(guid));
  setAuthKey(reinterpret_cast<char*>(authkey));
  commit();
  return true;
}

bool KeyValue::initFromMemory(uint8_t* input, size_t inputSize) {
  if (first) {
    // init can be done only on empty storage
    return false;
  }

  auto endPtr = input + inputSize;
  while (input + SUPLA_STORAGE_KEY_SIZE + 1 + 2 < endPtr) {
    char key[SUPLA_STORAGE_KEY_SIZE + 1] = {};
    memcpy(key, input, SUPLA_STORAGE_KEY_SIZE);
    input += SUPLA_STORAGE_KEY_SIZE;

    auto element = new KeyValueElement(key);
    switch (*input) {
      case DATA_TYPE_UINT8: {
        input++;     // dataType
        input += 2;  // skip size
        if (input < endPtr) {
          element->setUInt8(*input);
        } else {
          delete element;
          return false;
        }
        input++;
        break;
      }
      case DATA_TYPE_INT8: {
        input++;     // dataType
        input += 2;  // skip size
        if (input < endPtr) {
          element->setInt8(static_cast<int8_t>(*input));
        } else {
          delete element;
          return false;
        }
        input++;
        break;
      }
      case DATA_TYPE_UINT32: {
        input++;     // dataType
        input += 2;  // skip size
        if (input + 3 < endPtr) {
          uint32_t value;
          memcpy(&value, input, sizeof(value));
          element->setUInt32(value);
        } else {
          delete element;
          return false;
        }
        input += 4;
        break;
      }
      case DATA_TYPE_INT32: {
        input++;     // dataType
        input += 2;  // skip size
        if (input + 3 < endPtr) {
          int32_t value;
          memcpy(&value, input, sizeof(value));
          element->setInt32(value);
        } else {
          delete element;
          return false;
        }
        input += 4;
        break;
      }
      case DATA_TYPE_BLOB: {
        input++;  // dataType
        uint16_t size;
        memcpy(&size, input, sizeof(size));
        input += 2;  // size
        if (input + size - 1 < endPtr) {
          char* buffer = new char[size];
          memcpy(buffer, input, size);
          element->setBlob(buffer, size);
          delete[] buffer;
        } else {
          delete element;
          return false;
        }
        input += size;
        break;
      }
      case DATA_TYPE_STRING: {
        input++;  // dataType
        uint16_t size;
        memcpy(&size, input, sizeof(size));
        input += 2;  // size
        if (input + size - 1 < endPtr) {
          char* buffer = new char[size];
          memcpy(buffer, input, size);
          element->setString(buffer);
          delete[] buffer;
        } else {
          delete element;
          return false;
        }
        input += size;
        break;
      }
      default: {
        delete element;
        return false;
      }
    }
    if (!first) {
      first = element;
    } else {
      first->add(element);
    }
  }

  if (input < endPtr) {
    return false;
  }
  return true;
}

size_t KeyValue::serializeToMemory(uint8_t* output, size_t outputMaxSize) {
  size_t sizeCount = 0;
  for (auto element = first; element; element = element->getNext()) {
    size_t bytesWritten = element->serialize(output, outputMaxSize - sizeCount);
    output += bytesWritten;
    sizeCount += bytesWritten;
  }
  return sizeCount;
}

KeyValueElement* KeyValue::find(const char* key) {
  auto element = first;
  while (element) {
    if (element->isKeyEqual(key)) {
      return element;
    }
    element = element->getNext();
  }
  return nullptr;
}

KeyValueElement* KeyValue::findOrCreate(const char* key) {
  auto element = find(key);
  if (!element) {
    element = new KeyValueElement(key);
    element->add(first);
    first = element;
  }
  return element;
}

bool KeyValue::setString(const char* key, const char* value) {
  auto element = findOrCreate(key);
  return element->setString(value);
}

bool KeyValue::getString(const char* key, char* value, size_t maxSize) {
  auto element = find(key);
  if (!element) {
    return false;
  }
  return element->getString(value, maxSize);
}

int KeyValue::getStringSize(const char* key) {
  auto element = find(key);
  if (!element) {
    return 0;
  }
  return element->getStringSize();
}

bool KeyValue::setBlob(const char* key, const char* value, size_t blobSize) {
  auto element = findOrCreate(key);
  return element->setBlob(value, blobSize);
}

bool KeyValue::getBlob(const char* key, char* value, size_t blobSize) {
  auto element = find(key);
  if (!element) {
    return 0;
  }
  return element->getBlob(value, blobSize);
}

int KeyValue::getBlobSize(const char* key) {
  auto element = find(key);
  if (!element) {
    return 0;
  }
  return element->getBlobSize();
}

bool KeyValue::getInt8(const char* key, int8_t* result) {
  auto element = find(key);
  if (!element) {
    return 0;
  }
  return element->getInt8(result);
}

bool KeyValue::getUInt8(const char* key, uint8_t* result) {
  auto element = find(key);
  if (!element) {
    return 0;
  }
  return element->getUInt8(result);
}

bool KeyValue::getInt32(const char* key, int32_t* result) {
  auto element = find(key);
  if (!element) {
    return 0;
  }
  return element->getInt32(result);
}

bool KeyValue::getUInt32(const char* key, uint32_t* result) {
  auto element = find(key);
  if (!element) {
    return 0;
  }
  return element->getUInt32(result);
}

bool KeyValue::setInt8(const char* key, const int8_t value) {
  auto element = findOrCreate(key);
  return element->setInt8(value);
}

bool KeyValue::setUInt8(const char* key, const uint8_t value) {
  auto element = findOrCreate(key);
  return element->setUInt8(value);
}

bool KeyValue::setInt32(const char* key, const int32_t value) {
  auto element = findOrCreate(key);
  return element->setInt32(value);
}

bool KeyValue::setUInt32(const char* key, const uint32_t value) {
  auto element = findOrCreate(key);
  return element->setUInt32(value);
}

KeyValueElement::KeyValueElement(const char* keyName) {
  strncpy(key, keyName, SUPLA_STORAGE_KEY_SIZE);
}

KeyValueElement::~KeyValueElement() {
  if (size > 0 && data.charPtr && dataType == DATA_TYPE_STRING) {
    delete[] data.charPtr;
    size = 0;
    data.charPtr = nullptr;
  }

  if (size > 0 && data.uint8ptr && dataType == DATA_TYPE_BLOB) {
    delete[] data.uint8ptr;
    size = 0;
    data.uint8ptr = nullptr;
  }
}

bool KeyValueElement::isKeyEqual(const char* keyToCheck) {
  return strncmp(key, keyToCheck, SUPLA_STORAGE_KEY_SIZE) == 0;
}

KeyValueElement* KeyValueElement::getNext() {
  return next;
}

bool KeyValueElement::hasNext() {
  return next != nullptr;
}

void KeyValueElement::add(KeyValueElement* toBeAdded) {
  if (!hasNext()) {
    next = toBeAdded;
    return;
  }

  auto tmp = getNext();
  while (tmp->getNext()) {
    tmp = tmp->getNext();
  }
  tmp->add(toBeAdded);
}

void KeyValueElement::setNext(KeyValueElement* toBeSet) {
  next = toBeSet;
}

bool KeyValueElement::setString(const char* value) {
  unsigned int newSize = strlen(value) + 1;
  if (dataType == DATA_TYPE_NOT_SET) {
    dataType = DATA_TYPE_STRING;
    size = newSize;
    data.charPtr = new char[size];
  }
  if (dataType != DATA_TYPE_STRING) {
    return false;
  }
  if (newSize != size) {
    delete[] data.charPtr;
    size = newSize;
    data.charPtr = new char[size];
  }
  strncpy(data.charPtr, value, size);
  return true;
}

bool KeyValueElement::getString(char* value, size_t maxSize) {
  if (dataType != DATA_TYPE_STRING) {
    return false;
  }

  if (size <= maxSize) {
    strncpy(value, data.charPtr, maxSize);
    return true;
  }
  return false;
}

int KeyValueElement::getStringSize() {
  if (dataType != DATA_TYPE_STRING) {
    return 0;
  }
  return size;
}

bool KeyValueElement::setBlob(const char* value, size_t blobSize) {
  if (dataType == DATA_TYPE_NOT_SET) {
    dataType = DATA_TYPE_BLOB;
    size = blobSize;
    data.uint8ptr = new uint8_t[size];
  }
  if (dataType != DATA_TYPE_BLOB) {
    return false;
  }
  if (blobSize != size) {
    delete[] data.uint8ptr;
    size = blobSize;
    data.uint8ptr = new uint8_t[size];
  }
  memcpy(data.uint8ptr, value, blobSize);
  return true;
}

bool KeyValueElement::getBlob(char* value, size_t blobSize) {
  if (dataType != DATA_TYPE_BLOB) {
    return false;
  }

  if (size == blobSize) {
    memcpy(value, data.uint8ptr, blobSize);
    return true;
  }
  return false;
}

int KeyValueElement::getBlobSize() {
  if (dataType != DATA_TYPE_BLOB) {
    return 0;
  }
  return size;
}

bool KeyValueElement::getInt8(int8_t* result) {
  if (dataType == DATA_TYPE_INT8) {
    *result = data.int8;
    return true;
  }
  return false;
}

bool KeyValueElement::getUInt8(uint8_t* result) {
  if (dataType == DATA_TYPE_UINT8) {
    *result = data.uint8;
    return true;
  }
  return false;
}

bool KeyValueElement::getInt32(int32_t* result) {
  if (dataType == DATA_TYPE_INT32) {
    *result = data.int32;
    return true;
  }
  return false;
}

bool KeyValueElement::getUInt32(uint32_t* result) {
  if (dataType == DATA_TYPE_UINT32) {
    *result = data.uint32;
    return true;
  }
  return false;
}

bool KeyValueElement::setInt8(const int8_t value) {
  if (dataType == DATA_TYPE_NOT_SET) {
    dataType = DATA_TYPE_INT8;
  }

  if (dataType == DATA_TYPE_INT8) {
    data.int8 = value;
    return true;
  }

  return false;
}

bool KeyValueElement::setUInt8(const uint8_t value) {
  if (dataType == DATA_TYPE_NOT_SET) {
    dataType = DATA_TYPE_UINT8;
  }

  if (dataType == DATA_TYPE_UINT8) {
    data.uint8 = value;
    return true;
  }

  return false;
}

bool KeyValueElement::setInt32(const int32_t value) {
  if (dataType == DATA_TYPE_NOT_SET) {
    dataType = DATA_TYPE_INT32;
  }

  if (dataType == DATA_TYPE_INT32) {
    data.int32 = value;
    return true;
  }

  return false;
}

bool KeyValueElement::setUInt32(const uint32_t value) {
  if (dataType == DATA_TYPE_NOT_SET) {
    dataType = DATA_TYPE_UINT32;
  }

  if (dataType == DATA_TYPE_UINT32) {
    data.uint32 = value;
    return true;
  }

  return false;
}

size_t KeyValueElement::serialize(uint8_t* destination, size_t maxSize) {
  size_t blockSize = SUPLA_STORAGE_KEY_SIZE + 1 /* dataType */ + 2 /* size */
                     + size;
  switch (dataType) {
    case DATA_TYPE_INT8:
    case DATA_TYPE_UINT8: {
      blockSize += 1;
      break;
    }
    case DATA_TYPE_INT32:
    case DATA_TYPE_UINT32: {
      blockSize += 4;
      break;
    }
    default: {
      break;
    }
  }

  if (blockSize > maxSize) {
    SUPLA_LOG_ERROR("Key value too big");
    return 0;
  }

  memcpy(destination, key, SUPLA_STORAGE_KEY_SIZE);
  destination += SUPLA_STORAGE_KEY_SIZE;
  *destination = static_cast<uint8_t>(dataType);
  destination++;
  uint16_t rawSize = static_cast<uint16_t>(size);
  memcpy(destination, &rawSize, sizeof(rawSize));
  destination += sizeof(rawSize);

  switch (dataType) {
    case DATA_TYPE_INT8:
    case DATA_TYPE_UINT8: {
      *destination = data.uint8;
      break;
    }
    case DATA_TYPE_INT32: {
      memcpy(destination, &(data.int32), sizeof(data.int32));
      break;
    }
    case DATA_TYPE_UINT32: {
      memcpy(destination, &(data.uint32), sizeof(data.uint32));
      break;
    }
    case DATA_TYPE_BLOB:
    case DATA_TYPE_STRING: {
      memcpy(destination, data.uint8ptr, size);
      break;
    }
    default: {
      return 0;
    }
  }

  return blockSize;
}

bool KeyValue::eraseKey(const char* key) {
  auto elementToDelete = find(key);
  if (elementToDelete == nullptr) {
    return false;
  }

  // find previous:
  auto previous = first;
  while (previous) {
    if (previous->getNext() == elementToDelete) {
      previous->setNext(elementToDelete->getNext());
      break;
    }
    previous = previous->getNext();
  }
  delete elementToDelete;
  return true;
}

};  // namespace Supla
