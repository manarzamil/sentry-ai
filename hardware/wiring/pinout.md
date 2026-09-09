# Pinout and Wiring

All assignments below are read directly from `firmware/src/sentry_helmet.ino`.

## Pin map

| ESP32 pin | Direction | Connected to | Source in firmware |
|-----------|-----------|--------------|--------------------|
| GPIO 21 (SDA) | Bidirectional | MPU-6050 SDA | `Wire.begin()` — ESP32 I2C default |
| GPIO 22 (SCL) | Output | MPU-6050 SCL | `Wire.begin()` — ESP32 I2C default |
| GPIO 16 (RX2) | Input | NEO-6M **TX** | `gpsSerial.begin(9600, SERIAL_8N1, 16, 17)` |
| GPIO 17 (TX2) | Output | NEO-6M **RX** | `gpsSerial.begin(9600, SERIAL_8N1, 16, 17)` |
| GPIO 25 | Output | Active buzzer (+) | `const int BUZZER_PIN = 25;` |
| GPIO 4 | Input, pull-up | SOS push button to GND | `const int BUTTON_PIN = 4;` |
| GPIO 13 | Input, pull-up | Master toggle switch to GND | `const int SWITCH_PIN = 13;` |
| 3V3 / GND | Power | MPU-6050, NEO-6M, buzzer | — |

## Connection diagram

```mermaid
graph LR
    subgraph Power
        BAT["18650 Li-ion cell"] --> TP["TP4056 charger"]
        TP --> ESP
    end

    subgraph Sensors
        MPU["MPU-6050 IMU"]
        GPS["NEO-6M GPS"]
    end

    subgraph Controls
        SW["Toggle switch<br/>master on/off"]
        BTN["Push button<br/>manual SOS"]
    end

    MPU -- "SDA / GPIO 21" --> ESP["ESP32"]
    MPU -- "SCL / GPIO 22" --> ESP
    GPS -- "TX to GPIO 16" --> ESP
    ESP -- "GPIO 17 to RX" --> GPS
    SW -- "GPIO 13 to GND" --> ESP
    BTN -- "GPIO 4 to GND" --> ESP
    ESP -- "GPIO 25" --> BUZ["Active buzzer"]
```

## Wiring rules

Because both inputs rely on the ESP32's internal pull-up resistors, wire each switch and button as a simple closure to ground. An actuated control therefore reads `LOW`, which is how the firmware detects the transitions `HIGH -> LOW` for switch-on and SOS press.

Cross the GPS serial lines: the module's TX goes to the ESP32's RX (GPIO 16) and the module's RX goes to the ESP32's TX (GPIO 17). Wiring these straight through is the most common cause of a GPS module that never produces a fix.

## Photographs

_TODO: add wiring photographs and an annotated assembly image of the physical prototype. No hardware images were available in the source material._
