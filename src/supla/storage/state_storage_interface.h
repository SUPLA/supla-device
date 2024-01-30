/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifndef SRC_SUPLA_STORAGE_STATE_STORAGE_INTERFACE_H_
#define SRC_SUPLA_STORAGE_STATE_STORAGE_INTERFACE_H_

#include <stdint.h>

namespace Supla {

class Storage;

class StateStorageInterface {
 public:
  explicit StateStorageInterface(Storage *storage);
  virtual ~StateStorageInterface();
  virtual bool writeSectionPreamble() = 0;
  virtual bool initFromStorage() = 0;
  virtual void deleteAll() = 0;
  virtual bool prepareSaveState() = 0;
  virtual bool prepareSizeCheck() = 0;
  virtual bool prepareLoadState() = 0;
  virtual bool readState(unsigned char *, int) = 0;
  virtual bool writeState(const unsigned char *, int) = 0;
  virtual bool finalizeSaveState() = 0;
  virtual bool finalizeSizeCheck() = 0;
  virtual void notifyUpdate();

 protected:
  int readStorage(unsigned int address,
                  unsigned char *buf,
                  int size,
                  bool logs = true);
  int writeStorage(unsigned int address, const unsigned char *buf, int size);
  int updateStorage(unsigned int address, const unsigned char *buf, int size);
  void commit();

  Storage *storage = nullptr;
};
}  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_STATE_STORAGE_INTERFACE_H_
