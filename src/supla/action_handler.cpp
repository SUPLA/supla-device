// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "action_handler.h"
#include "local_action.h"

Supla::ActionHandler::~ActionHandler() {
  Supla::LocalAction::NullifyActionsHandledBy(this);
}

void Supla::ActionHandler::activateAction(int action) {
  (void)(action);
}

bool Supla::ActionHandler::deleteClient() {
  return false;
}

Supla::ActionHandler *Supla::ActionHandler::getRealClient() {
  return this;
}
