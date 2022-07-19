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

#include "channel_element.h"
#include "events.h"

Supla::Channel *Supla::ChannelElement::getChannel() {
  return &channel;
}

void Supla::ChannelElement::addAction(int action,
    ActionHandler &client,
    int event) {
  channel.addAction(action, client, event);
}

void Supla::ChannelElement::addAction(int action,
    ActionHandler *client,
    int event) {
  addAction(action, *client, event);
}

bool Supla::ChannelElement::isEventAlreadyUsed(int event) {
  return channel.isEventAlreadyUsed(event);
}

void Supla::ChannelElement::addAction(int action,
    ActionHandler &client,
    Supla::Condition *condition) {
  condition->setClient(client);
  condition->setSource(this);
  channel.addAction(action, condition, Supla::ON_CHANGE);
}

void Supla::ChannelElement::addAction(int action,
    ActionHandler *client,
    Supla::Condition *condition) {
  addAction(action, *client, condition);
}

