import serial
from pydbus import SessionBus
from gi.repository import GLib

SERIAL_PORT = "/dev/ttyACM0"  # Adjust for your Arduino device
BAUD_RATE = 115200

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    ser = None


def on_message(connection, sender, path, iface, member, params):
    # Filter strictly for the Notify method call
    if member != "Notify" or iface != "org.freedesktop.Notifications":
        return

    # FreeDesktop Notify signature: (susssasa{sv}i)
    # params: [app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout]
    args = params.unpack()
    app_name = args[0] or "System"
    summary = args[3] or ""
    body = args[4] or ""

    # Sanitize newlines to preserve line-delimited serial protocol
    summary = summary.replace("\n", " ").strip()
    body = body.replace("\n", " ").strip()

    payload = f"[{app_name}] {summary} - {body}\n"
    print(f"Captured: {payload.strip()}")

    if ser and ser.is_open:
        ser.write(payload.encode("utf-8", errors="replace"))


def main():
    bus = SessionBus()

    # Become a bus monitor for notifications
    # Match calls to org.freedesktop.Notifications.Notify
    match_rule = (
        "type='method_call',"
        "interface='org.freedesktop.Notifications',"
        "member='Notify'"
    )

    # Low-level GDBusConnection subscription
    bus.con.add_filter(
        lambda conn, msg, user_data: on_message(
            conn,
            msg.get_sender(),
            msg.get_path(),
            msg.get_interface(),
            msg.get_member(),
            msg.get_body(),
        )
    )

    # Request bus monitoring matching the rule
    bus.dbus.Monitoring.BecomeMonitor([match_rule], 0)

    print("Listening for KDE Plasma notifications...")
    loop = GLib.MainLoop()
    try:
        loop.run()
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        if ser and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()