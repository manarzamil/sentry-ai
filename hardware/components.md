# Hardware Components

Bill of materials for the Sentry AI prototype. Every entry below is taken from the project build documentation and cross-checked against the pin assignments in `firmware/src/sentry_helmet.ino`.

## Bill of materials

| Component | Role in the system | Interface | Assignment |
|-----------|--------------------|-----------|------------|
| **ESP32** | Main controller. Executes sensor fusion, fall detection, fatigue scoring and hosts the Wi-Fi Access Point and HTTP server | — | — |
| **MPU-6050** | 6-axis IMU (3-axis accelerometer + 3-axis gyroscope). Primary input for fall detection, tilt estimation and helmet-worn detection | I2C | SDA = GPIO 21, SCL = GPIO 22 |
| **NEO-6M GPS** | Global positioning. Supplies latitude/longitude for incident geotagging and hazard-zone mapping | UART2 @ 9600 baud | RX2 = GPIO 16, TX2 = GPIO 17 |
| **Active buzzer** | Local audible alarm for fall, SOS and fatigue events | Digital output | GPIO 25 |
| **Momentary push button** | Manual SOS trigger | Digital input, internal pull-up | GPIO 4 |
| **Toggle switch** | Master monitoring on/off control | Digital input, internal pull-up | GPIO 13 |
| **18650 Li-ion cell** | Portable power source | — | — |
| **TP4056 module** | Li-ion charge management for the 18650 cell | — | — |

## Notes on the sensor configuration

The MPU-6050 is initialised with a digital low-pass filter bandwidth of 44 Hz (`MPU6050_BAND_44_HZ`), which attenuates high-frequency vibration that would otherwise produce spurious acceleration peaks during normal work activity.

The GPS module communicates over the ESP32's second hardware UART. In the ESP32 Arduino core the pin order for `HardwareSerial::begin` is *(baud, config, RX, TX)*, so GPIO 16 is the ESP32 receive line and must be wired to the **TX** pad of the NEO-6M.

Both the SOS button and the master switch use `INPUT_PULLUP`, so each is wired directly between its GPIO and ground and reads `LOW` when actuated. No external pull-up resistors are required.

## Not yet documented

The following are not covered by the current project files and are listed here as open items rather than assumed:

- Enclosure design and helmet mounting method
- Measured current draw and battery endurance
- Voltage regulation between the 18650 cell and the ESP32
- Antenna placement for the GPS module
