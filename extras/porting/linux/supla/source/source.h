// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SOURCE_SOURCE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SOURCE_SOURCE_H_

#include <string>
#include <vector>

namespace Supla {

namespace Source {
// A document snapshot owns its payload. received distinguishes an empty message
// from no message; fresh reflects optional source-specific expiration.
struct Document {
  std::string id;
  std::string content;
  bool received = false;
  bool fresh = true;
};

class Source {
 public:
  virtual ~Source() {}
  virtual std::string getContent() = 0;
  // Ordinary sources expose one document named "default". Their legacy API
  // cannot distinguish missing from empty payloads; both fail JSON parsing.
  virtual std::vector<Document> getDocuments() {
    return {{"default", getContent(), true, true}};
  }
  virtual std::vector<std::string> getDocumentIds() const {
    return {"default"};
  }
  virtual bool isConnected() { return true; }
};
};  // namespace Source
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SOURCE_SOURCE_H_
