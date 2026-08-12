// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONDITION_H_
#define SRC_SUPLA_CONDITION_H_

#include "action_handler.h"
#include "condition_getter.h"

namespace Supla {

class ElementWithChannelActions;

class Condition : public ActionHandler {
 public:
  Condition(double threshold, bool useAlternativeValue);
  Condition(double threshold, ConditionGetter *getter);

  virtual ~Condition();
  void setSource(ElementWithChannelActions *src);
  void setClient(ActionHandler *clientPtr);
  void setSource(ElementWithChannelActions &src);  // NOLINT(runtime/references)
  void setClient(ActionHandler &clientPtr);  // NOLINT(runtime/references)

  void activateAction(int action) override;
  void handleAction(int event, int action) override;
  bool deleteClient() override;
  ActionHandler *getRealClient() override;

  virtual bool checkConditionFor(double val, bool isValid = true);

  void setThreshold(double val);

 protected:
  virtual bool condition(double val, bool isValid = true) = 0;

  double threshold = 0;
  bool useAlternativeValue = false;
  bool alreadyFired = false;
  Supla::ElementWithChannelActions *source = nullptr;
  Supla::ActionHandler *client = nullptr;
  Supla::ConditionGetter *getter = nullptr;
};

};  // namespace Supla

Supla::Condition *OnLess(double threshold,
    bool useAlternativeValue = false);
Supla::Condition *OnLessEq(double threshold,
    bool useAlternativeValue = false);
Supla::Condition *OnGreater(double threshold,
    bool useAlternativeValue = false);
Supla::Condition *OnGreaterEq(double threshold,
    bool useAlternativeValue = false);
Supla::Condition *OnBetween(double threshold1,
    double threshold2,
    bool useAlternativeValue = false);
Supla::Condition *OnBetweenEq(double threshold1,
    double threshold2,
    bool useAlternativeValue = false);
Supla::Condition *OnEqual(double threshold,
    bool useAlternativeValue = false);
Supla::Condition *OnInvalid(bool useAlternativeValue = false);

Supla::Condition *OnLess(double threshold, Supla::ConditionGetter *);
Supla::Condition *OnLessEq(double threshold, Supla::ConditionGetter *);
Supla::Condition *OnGreater(double threshold, Supla::ConditionGetter *);
Supla::Condition *OnGreaterEq(double threshold, Supla::ConditionGetter *);
Supla::Condition *OnBetween(double threshold1,
    double threshold2,
    Supla::ConditionGetter *);
Supla::Condition *OnBetweenEq(double threshold1,
    double threshold2,
    Supla::ConditionGetter *);
Supla::Condition *OnEqual(double threshold, Supla::ConditionGetter *);
Supla::Condition *OnInvalid(Supla::ConditionGetter *);

#endif  // SRC_SUPLA_CONDITION_H_
