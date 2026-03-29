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
/dps/
    console_dps.py          # Python simulator with automatic DPS provisioning
    derived.py           # Helper script to calculate the device derived key
    requirements.txt
/esp32_dps/
    esp32_dps.ino           # Arduino sketch for ESP32 with automatic DPS provisioning
/command_hub/
    .deployment
    app.py                  # Flask application for sending commands
    requirements.txt
/gemelos_hub/
    # Several examples using Azure digital twins
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

## DPS — Automatic provisioning (Python)

Demonstrates how a device can register itself automatically in Azure IoT Hub using **Azure IoT Hub Device Provisioning Service (DPS)** with Symmetric Key attestation. No hardcoded connection string — the device contacts DPS at startup and receives its IoT Hub assignment dynamically.

### Requirements

```bash
pip install -r dps/requirements.txt
```

### Configuration

Edit the following constants at the top of `console_dps.py`:

```python
PROVISIONING_HOST = "global.azure-devices-provisioning.net"
ID_SCOPE          = "0ne00XXXXXX"       # from the DPS portal (Overview)
REGISTRATION_ID   = "mydevice-dps"      # must match the ID used to derive the key
DEVICE_SYMMETRIC_KEY = "..."            # derived key (see derived.py)
```

### Derive the device key

Before running the simulator, calculate the device's derived key from the enrollment group primary key:

```bash
python dps/derive_key.py
```

Edit `derive_key.py` with your `GROUP_PRIMARY_KEY` and `REGISTRATION_ID` and copy the output into `console_dps.py`.

### Run

```bash
python dps/console_dps.py
```

The device will appear automatically under **IoT Hub → Devices** without any prior manual registration.

---

## ESP32 DPS — Automatic provisioning (Arduino)

ESP32 variant of the DPS example. Uses the device's factory **MAC address** as `REGISTRATION_ID`, so a single firmware works for any number of devices without modification.

### How it works

- The MAC address is read after `WiFi.begin()` and used as the unique device identifier
- The derived key is calculated on the ESP32 itself using mbedTLS (included in the ESP32 Arduino framework) — no need to run the Python helper script
- The device contacts DPS via MQTT, receives its IoT Hub assignment, and connects normally

### Configuration

Edit only these two constants in `esp32_dps.ino`:

```cpp
const char* ID_SCOPE  = "0ne00XXXXXX";      // from the DPS portal
const char* GROUP_KEY = "<primary-key>";     // from the enrollment group
```

These are the same for all devices in the group. Everything else is generated automatically at boot.

### Notes

- `PubSubClient::publish()` requires `const char*` — build payloads with `snprintf()`, not Arduino `String` concatenation
- Read the MAC with `WiFi.macAddress()` after `WiFi.begin()` — reading before WiFi initialisation returns zeros
- The DigiCert Global Root G2 certificate (same as `esp32_iothub`) is valid for both DPS and IoT Hub endpoints

---

## command_hub

Flask application for sending commands to IoT Hub

```python
@app.route('/command')  # sends command to IoT Hub
```

---

## gemelos_hub

Several examples showing how to use Azure digital twins: desired and reported properties, query twins, etc.
