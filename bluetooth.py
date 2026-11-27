
from bleak import BleakClient, BleakScanner

# SERVICE_UUID = "01a219a4-df08-71be-b29e-15f1d0017ddf"
CHARACTERISTIC_UUID_RX = "01a219a5-0c99-73f9-9fca-973d5e5b121d"

async def get_client():
    device = await BleakScanner.find_device_by_name("ESP32_Bluetooth", timeout=3)
    if not device:
        print(await BleakScanner.discover())
        raise Exception("Could not connect to ESP32")  # noqa: TRY002
    client = BleakClient(device)
    await client.connect()
    print("Bleak connected to ESP32")
    return client

async def send_data(client: BleakClient, data: list[int]):
    if not client.is_connected:
        print("Client is not connected, skipping")
        return
    print(bytes(data))
    resp = await client.write_gatt_char(CHARACTERISTIC_UUID_RX, bytes(data))
    print(resp)
