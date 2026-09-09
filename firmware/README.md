# Firmware

ESP32 firmware for the Sentry AI smart safety helmet, written against the Arduino framework.

## Layout

| Path | Purpose |
|------|---------|
| `src/sentry_helmet.ino` | Complete application: sensor acquisition, fall detection, fatigue scoring, GPS handling, persistence, buzzer control and HTTP server |
| `src/config.example.h` | Configuration template. Copy to `config.h` and set your own SoftAP credentials |

## Prerequisites

- Arduino IDE 2.x (or Arduino CLI)
- ESP32 board support package (Espressif Systems)

### Libraries

Install through the Arduino Library Manager:

| Library | Used for |
|---------|----------|
| Adafruit MPU6050 | IMU driver |
| Adafruit Unified Sensor | Sensor abstraction required by the MPU6050 driver |
| TinyGPSPlus | NMEA sentence parsing |
| Preferences | Non-volatile storage (bundled with the ESP32 core, no installation required) |

## Build and flash

1. Create your configuration file:

   ```bash
   cd firmware/src
   cp config.example.h config.h
   ```

2. Edit `config.h` and replace `CHANGE_ME_SSID` and `CHANGE_ME_STRONG_PASSWORD`. WPA2 requires at least 8 characters.

3. Open `sentry_helmet.ino` in the Arduino IDE. Arduino expects the sketch folder name to match the sketch name; if the IDE objects, place `sentry_helmet.ino` and `config.h` together in a folder named `sentry_helmet`.

4. Select your ESP32 board and serial port, then upload.

5. Open the Serial Monitor at **115200 baud**. On success the firmware reports MPU-6050 initialisation, GPS readiness, and the SoftAP IP address.

## First run

Connect a phone or laptop to the Wi-Fi network named by your `SSID` value, then browse to the IP printed on the serial monitor (the ESP32 SoftAP default is `192.168.4.1`).

Monitoring is gated by the hardware switch on GPIO 13. The dashboard shows `SYSTEM OFF` until the switch is closed; closing it triggers the startup beep sequence and begins the sensor loop.

## Startup diagnostics

If the MPU-6050 cannot be reached over I2C the firmware prints `[ERROR] MPU6050 not found!` and enters a permanent 200 ms buzzer loop rather than continuing with invalid motion data. Check the SDA/SCL wiring described in `../hardware/wiring/pinout.md`.

## Security note

Never commit `config.h`. It is excluded by the repository `.gitignore`. See `../SECURITY.md` for the full set of known security considerations.
