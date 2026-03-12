# client_azure_hub

Examples of IoT devices sending telemetry to **Azure IoT Hub** and receiving cloud-to-device (C2D) commands. Includes Python and ESP32 (Arduino) implementations.

All examples simulate temperature, humidity and pressure sensors and share the same structure:
- Send telemetry every few seconds
- Listen for and execute commands from the cloud

---

## Repository structure

```
/python/
    console_iothub.py       # Random values, runs on any OS
    sensehat_iothub.py      # Uses SenseHat Emulator (Raspberry Pi / desktop)
    requirements.txt
/esp32_iothub/
    esp32_iothub.ino        # Arduino sketch for ESP32
    SETUP.md                # Setup instructions (certificates, SAS token, etc.)
/command_hub/
    .deployment
    app.py                  # Flask application for sending commands
    requirements.txt
README.md
```

---

## Python

Tested on Windows 11, Debian 11 and Raspberry Pi 3 (Bullseye).

### Requirements

```bash
pip install -r python/requirements.txt
```

### Configuration

Edit the connection string at the top of each script:

```python
CONNECTION_STRING = "HostName=YOURHUB.azure-devices.net;DeviceId=YOURDEVICE;SharedAccessKey=..."
```

### Run

```bash
# Console version (random values)
python python/console_iothub.py

# SenseHat Emulator version
python python/sensehat_iothub.py
```

---

## ESP32 (Arduino)

Connects to Azure IoT Hub via **MQTT over TLS** (port 8883) using the `PubSubClient` library. No Azure SDK required.

See [`esp32_iothub/SETUP.md`](esp32_iothub/SETUP.md) for full setup instructions including certificate download, SAS token generation and sending C2D commands.

---

## command_hub

Flask application for sending commands to IoT Hub

```python
@app.route('/command')  # sends command to IoT Hub
```

