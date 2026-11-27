"""Desktop Notification Listener for ESP32 Bluetooth (ECE 2360).

Listens for Linux desktop notifications over D-Bus, classifies them into alert
categories (WARNING, SUCCESS, INFO, LOW), and prints the notification details
along with the corresponding Bluetooth signal.

Optionally transmits the signal to an ESP32 via Bluetooth RFCOMM or Serial.
"""

import asyncio

from bleak import BleakClient
from dbus_fast import Message, MessageType
from dbus_fast.aio import MessageBus

from bluetooth import get_client
from handlers import INTERFACES, Rule


async def add_match_rule(bus: MessageBus, rules: list[Rule]):
    reply = await bus.call(
        Message(
            destination="org.freedesktop.DBus",
            path="/org/freedesktop/DBus",
            interface="org.freedesktop.DBus.Monitoring",
            member="BecomeMonitor",
            signature="asu",
            body=[
                [
                    f"path_namespace='{rule.path}',member='{rule.member}'"
                    for rule in rules
                ],
                0,
            ],
        )
    )
    assert reply.message_type == MessageType.METHOD_RETURN


def filtered_handler(m: Message, bt_cli: BleakClient):
    async def handler():
        for rule in INTERFACES:
            if m.path and rule.path in m.path and rule.member == m.member:
                return await rule.handler(m, bt_cli)

    asyncio.create_task(handler())

    return True


async def main():
    bus = await MessageBus().connect()
    bt_cli = await get_client()

    # start all of the requests then wait for them all to finish
    await add_match_rule(bus, INTERFACES)

    bus.add_message_handler(lambda m: filtered_handler(m, bt_cli))

    await bus.wait_for_disconnect()


if __name__ == "__main__":
    asyncio.run(main())
