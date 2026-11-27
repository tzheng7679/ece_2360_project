# ESP32 Bluetooth Desktop Notification Listener

A lightweight Python script that intercepts Linux desktop notifications (KDE Plasma, GNOME, etc.) over D-Bus, classifies them, and outputs the corresponding Bluetooth alert signals for an ESP32 microcontroller controlling an RGB LED and audio buzzer.

---

## Quick Start

### 1. Run the Listener (Terminal Preview)

To listen for incoming notifications and print both the notification details and corresponding Bluetooth signal in your terminal:

```bash
uv run python listen.py
```

### 2. Test with Sample Notifications

To dispatch sample test notifications (`INFO`, `SUCCESS`, `WARNING`) using `notify-send` right after startup:

```bash
uv run python listen.py --test
```

### 3. Send over Bluetooth to ESP32

To transmit alert signals directly to your ESP32's Bluetooth MAC address:

```bash
uv run python listen.py --mac 24:6F:28:XX:XX:XX
```

### 4. Send over USB Serial / RFCOMM Device

```bash
uv run python listen.py --port /dev/ttyACM0
```

---

## Signal Mapping

| Notification Type | Trigger Condition | ESP32 LED Color | Sound Alert | Bluetooth Signal Sent |
|---|---|---|---|---|
| **`WARNING`** | Critical urgency, error/fail/low battery keywords | Red | Alarm whine tone | `WARNING\n` |
| **`SUCCESS`** | Transfer complete, success/done keywords | Green | Pleasant chime | `SUCCESS\n` |
| **`INFO`** | Normal desktop notifications | Blue | Standard beep | `INFO\n` |
| **`LOW`** | Low urgency background notices | Amber | Low chirp | `LOW\n` |

---

## Firmware

The ESP32 Arduino sketch is available at:
[`examples/esp32_notification_receiver.ino`](examples/esp32_notification_receiver.ino)
