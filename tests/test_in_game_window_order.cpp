/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "openkal/client/InGameWindowOrder.h"

#include <cassert>

int main() {
    using openkal::client::InGameWindowId;
    using openkal::client::InGameWindowOrder;

    InGameWindowOrder order;
    assert(!order.latest().has_value());

    order.opened(InGameWindowId::Inventory);
    assert(order.latest() == InGameWindowId::Inventory);

    order.opened(InGameWindowId::Skills);
    order.opened(InGameWindowId::CharacterInfo);
    assert(order.latest() == InGameWindowId::CharacterInfo);

    order.closed(InGameWindowId::CharacterInfo);
    assert(order.latest() == InGameWindowId::Skills);

    order.opened(InGameWindowId::Inventory);
    assert(order.latest() == InGameWindowId::Inventory);

    order.closed(InGameWindowId::Inventory);
    assert(order.latest() == InGameWindowId::Skills);

    order.closed(InGameWindowId::Skills);
    assert(!order.latest().has_value());

    order.opened(InGameWindowId::Transform);
    order.opened(InGameWindowId::Riding);
    assert(order.latest() == InGameWindowId::Riding);
    order.closed(InGameWindowId::Riding);
    assert(order.latest() == InGameWindowId::Transform);

    order.opened(InGameWindowId::NpcTalk);
    order.opened(InGameWindowId::Riding);
    order.opened(InGameWindowId::Inventory);
    assert(order.latest() == InGameWindowId::Inventory);
    order.closed(InGameWindowId::Inventory);
    assert(order.latest() == InGameWindowId::Riding);
    order.opened(InGameWindowId::Riding);
    order.closed(InGameWindowId::Riding);
    assert(order.latest() == InGameWindowId::NpcTalk);
    order.closed(InGameWindowId::NpcTalk);
    order.closed(InGameWindowId::Transform);
    assert(!order.latest().has_value());
    return 0;
}
