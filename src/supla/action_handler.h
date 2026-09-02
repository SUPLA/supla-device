// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_ACTION_HANDLER_H_
#define SRC_SUPLA_ACTION_HANDLER_H_

namespace Supla {
class ActionHandler {
 public:
  virtual ~ActionHandler();
  virtual void handleAction(int event, int action) = 0;
  virtual void activateAction(int action);
  virtual bool deleteClient();
  virtual ActionHandler *getRealClient();
};

}  // namespace Supla
#endif  // SRC_SUPLA_ACTION_HANDLER_H_
