#!/usr/bin/env python3
"""Desktop Notification Listener for ESP32 Bluetooth (ECE 2360).

Listens for Linux desktop notifications over D-Bus, classifies them into alert
categories (WARNING, SUCCESS, INFO, LOW), and prints the notification details
along with the corresponding Bluetooth signal.

Optionally transmits the signal to an ESP32 via Bluetooth RFCOMM or Serial.
"""

import argparse
import asyncio
import os
import socket
import subprocess
import sys
from typing import Any, Dict, Optional

from dbus_fast import Variant
from dbus_fast.aio import MessageBus
from dbus_fast.constants import BusType, MessageType
from dbus_fast.message import Message


def unpack_variant(val: Any) -> Any:
    """Recursively unpack dbus_fast Variant instances to native Python objects."""
    if isinstance(val, Variant):
        return unpack_variant(val.value)
    if isinstance(val, dict):
        return {k: unpack_variant(v) for k, v in val.items()}
    if isinstance(val, list):
        return [unpack_variant(v) for v in val]
    return val


def get_session_bus_address() -> Optional[str]:
    """Detect D-Bus session bus address from environment or systemd socket."""
    addr = os.environ.get("DBUS_SESSION_BUS_ADDRESS")
    if addr:
        return addr
    try:
        user_bus = f"/run/user/{os.getuid()}/bus"
        if os.path.exists(user_bus):
            return f"unix:path={user_bus}"
    except Exception:
        pass
    return None


def classify_notification(summary: str, body: str, hints: Dict[str, Any]) -> str:
    """Classify notification into WARNING, SUCCESS, INFO, or LOW."""
    # 1. Urgency hint: 0 = low, 1 = normal, 2 = critical
    urgency = hints.get("urgency", 1)
    try:
        urgency = int(urgency)
    except (ValueError, TypeError):
        urgency = 1

    if urgency == 2:
        return "WARNING"
    if urgency == 0:
        return "LOW"

    # 2. Category hint
    category = str(hints.get("category", "")).lower()
    if "transfer.complete" in category or "complete" in category:
        return "SUCCESS"
    if "error" in category or "device.error" in category:
        return "WARNING"

    # 3. Keyword matching in summary and body
    text = f"{summary} {body}".lower()

    warning_words = [
        "error", "fail", "failed", "failure", "warning",
        "warn", "critical", "alert", "battery low", "danger",
    ]
    if any(w in text for w in warning_words):
        return "WARNING"

    success_words = [
        "success", "complete", "completed", "finish",
        "finished", "done", "downloaded", "saved", "passed",
    ]
    if any(w in text for w in success_words):
        return "SUCCESS"

    return "INFO"


class BluetoothSender:
    """Direct Bluetooth RFCOMM socket connection to ESP32."""

    def __init__(self, mac_address: Optional[str], channel: int = 1):
        self.mac = mac_address.strip() if mac_address else None
        self.channel = channel
        self.sock: Optional[socket.socket] = None

    def connect(self) -> bool:
        if not self.mac:
            return False
        if not hasattr(socket, "AF_BLUETOOTH"):
            print("Notice: AF_BLUETOOTH is not supported on this platform.", file=sys.stderr)
            return False
        try:
            print(f"Connecting to ESP32 at Bluetooth MAC {self.mac} (channel {self.channel})...")
            sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
            sock.settimeout(5.0)
            sock.connect((self.mac, self.channel))
            sock.settimeout(None)
            self.sock = sock
            print("Successfully connected to ESP32 over Bluetooth!")
            return True
        except Exception as err:
            print(f"Bluetooth connection failed ({err}). Operating in preview mode.", file=sys.stderr)
            self.sock = None
            return False

    def send(self, signal: str) -> None:
        if not self.sock:
            return
        try:
            payload = f"{signal}\n".encode("utf-8")
            self.sock.sendall(payload)
        except Exception as err:
            print(f"Bluetooth send error: {err}", file=sys.stderr)
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None

    def close(self) -> None:
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None


def handle_notification(
    app_name: str,
    summary: str,
    body: str,
    hints: Dict[str, Any],
    bt_sender: Optional[BluetoothSender],
) -> None:
    """Print notification, print bluetooth signal, and transmit if connected."""
    app_name = app_name.strip() or "System"
    summary = summary.replace("\n", " ").strip()
    body = body.replace("\n", " ").strip()

    signal = classify_notification(summary, body, hints)

    print("-" * 55)
    print("Captured Notification:")
    print(f"  App:     {app_name}")
    print(f"  Summary: {summary}")
    if body:
        print(f"  Body:    {body}")
    print(f"Bluetooth Signal: {signal}")
    print("-" * 55, flush=True)

    if bt_sender:
        bt_sender.send(signal)


def send_test_notifications() -> None:
    """Helper to dispatch sample notifications via notify-send for testing."""
    print("\n--- Dispatching sample test notifications via notify-send ---")
    demos = [
        ("notify-send", ["Low Battery Alert", "Battery level is below 10%! Please connect charger.", "--urgency=critical"]),
        ("notify-send", ["File Transfer Complete", "Downloaded final_report.pdf successfully", "--urgency=normal"]),
        ("notify-send", ["System Notice", "Software updates available", "--urgency=low"]),
    ]
    for prog, args in demos:
        try:
            subprocess.run([prog] + args, check=False)
        except FileNotFoundError:
            print("Note: 'notify-send' utility not found. Install 'libnotify' to send test notifications.")
            break


async def listen(
    mac: Optional[str] = None,
    channel: int = 1,
    port: Optional[str] = None,
    baud: int = 115200,
    test: bool = False,
) -> None:
    """Connect to D-Bus, monitor notifications, and output Bluetooth signals."""
    bt_sender = BluetoothSender(mac, channel=channel) if mac else None

    if bt_sender:
        bt_sender.connect()

    bus_address = get_session_bus_address()
    if bus_address:
        bus = MessageBus(bus_address=bus_address)
    else:
        bus = MessageBus(bus_type=BusType.SESSION)

    await bus.connect()

    def on_dbus_message(msg: Message) -> bool:
        if (
            msg.message_type == MessageType.METHOD_CALL
            and msg.interface == "org.freedesktop.Notifications"
            and msg.member == "Notify"
        ):
            try:
                body = msg.body
                app_name = body[0] if len(body) > 0 and body[0] else "System"
                summary = body[3] if len(body) > 3 else ""
                body_text = body[4] if len(body) > 4 else ""
                raw_hints = body[6] if len(body) > 6 else {}
                hints = unpack_variant(raw_hints)

                handle_notification(app_name, summary, body_text, hints, bt_sender, ser_sender)
            except Exception as err:
                print(f"Error processing notification: {err}", file=sys.stderr)
        return False

    bus.add_message_handler(on_dbus_message)

    match_rule = "type='method_call',interface='org.freedesktop.Notifications',member='Notify'"
    monitor_msg = Message(
        destination="org.freedesktop.DBus",
        path="/org/freedesktop/DBus",
        interface="org.freedesktop.DBus.Monitoring",
        member="BecomeMonitor",
        signature="asu",
        body=[[match_rule], 0],
    )
    await bus.call(monitor_msg)

    print("=======================================================")
    print(" Listening for Linux Desktop Notifications...")
    print(" Captured notifications & Bluetooth signals will print below.")
    if mac:
        print(f" Target Bluetooth ESP32: {mac} (ch {channel})")
    if port:
        print(f" Target Serial Port:     {port}")
    print(" Press Ctrl+C to stop.")
    print("=======================================================\n", flush=True)

    if test:
        asyncio.get_running_loop().call_later(0.5, send_test_notifications)

    stop_event = asyncio.Event()
    try:
        await stop_event.wait()
    except asyncio.CancelledError:
        pass
    finally:
        bus.disconnect()
        if bt_sender:
            bt_sender.close()
        if ser_sender:
            ser_sender.close()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Listen for desktop notifications and print them and their corresponding Bluetooth signal.",
    )
    parser.add_argument(
        "--mac",
        default=None,
        help="Optional ESP32 Bluetooth MAC address (e.g. 24:6F:28:XX:XX:XX) to transmit signal over Bluetooth.",
    )
    parser.add_argument(
        "--channel",
        type=int,
        default=1,
        help="Bluetooth RFCOMM channel number (default: 1).",
    )
    parser.add_argument(
        "--port",
        default=None,
        help="Optional serial port (e.g. /dev/ttyACM0 or /dev/rfcomm0) to transmit signal over serial.",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Serial baud rate (default: 115200).",
    )
    parser.add_argument(
        "--test",
        action="store_true",
        help="Dispatch sample test notifications using notify-send on startup.",
    )

    args = parser.parse_args()

    try:
        asyncio.run(
            listen(
                mac=args.mac,
                channel=args.channel,
                port=args.port,
                baud=args.baud,
                test=args.test,
            )
        )
    except KeyboardInterrupt:
        print("\nListener stopped.")


if __name__ == "__main__":
    main()
