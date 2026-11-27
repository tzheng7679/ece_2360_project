from collections.abc import Callable
from dataclasses import dataclass

from bleak import BleakClient
from dbus_fast import Message

from bluetooth import send_data
from consts import (
    FAILURE,
    GREEN,
    NEUTRAL,
    PURPLE,
    RED,
    SILENT,
    SUCCESS,
    WHITE,
    Goodness,
)


async def start_job_handler(m: Message, bt_cli: BleakClient):
    """IDK what other things this is used for, but I intend this to check for file transfers starting"""
    await send_data(bt_cli, WHITE + SUCCESS)
    print(m)
    return True


async def update_job_handler(m: Message, bt_cli: BleakClient):
    """For when a job progress update comes in"""
    print("job updated")
    await send_data(bt_cli, PURPLE + SILENT)
    print(m)
    return True


async def terminate_job_handler(m: Message, bt_cli: BleakClient):
    """For when a job finishes"""
    print("job terminated")
    err_code, error_message, _ = m.body
    if err_code == 0: # completed normally
        data = GREEN + SUCCESS
    else:
        print(error_message)
        data = RED + FAILURE

    await send_data(bt_cli, data)
    print(m)
    return True


@dataclass
class Rule:
    path: str
    member: str
    handler: Callable


async def static_notification_handler(m: Message, bt_cli: BleakClient):
    _app, _, _, summary, body, _, _hints, _ = m.body
    sg = Goodness.of(summary)
    bg = Goodness.of(body)
    if bg == Goodness.GOOD or sg == Goodness.GOOD:
        # send good
        data = GREEN + SUCCESS
    elif bg == Goodness.BAD or sg == Goodness.BAD:
        print("bad")
        data = RED + FAILURE
    else:
        data = WHITE + NEUTRAL

    await send_data(bt_cli, data)

    return True


INTERFACES = [
    Rule("/org/freedesktop/Notifications", "Notify", static_notification_handler),
    Rule("/org/kde/notificationmanager", "requestView", start_job_handler),
    Rule("/org/kde/notificationmanager", "update", update_job_handler),
    Rule("/org/kde/notificationmanager", "terminate", terminate_job_handler),
]