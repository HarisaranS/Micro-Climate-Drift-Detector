# ESP32 Drift Monitor

This project reads temperature and humidity from a DHT sensor, shows live values on an I2C LCD, and uploads data to Firebase Realtime Database.

## Minimum Hardware (Recommended)

- ESP32
- DHT11 or DHT22 sensor
- 16x2 LCD with I2C backpack (PCF8574)
- Breadboard
- Jumper wires
- 10k ohm resistor
- USB cable

## Wiring

### 1) DHT11/DHT22 -> ESP32

- DHT VCC -> ESP32 3V3
- DHT GND -> ESP32 GND
- DHT DATA -> ESP32 GPIO4
- 10k resistor between DHT DATA and 3V3 (pull-up)

### 2) I2C LCD -> ESP32

- LCD VCC -> ESP32 5V (or 3V3 if your module supports it)
- LCD GND -> ESP32 GND
- LCD SDA -> ESP32 GPIO21
- LCD SCL -> ESP32 GPIO22

## Arduino IDE Setup

Install these libraries in Arduino IDE Library Manager:

- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- LiquidCrystal I2C

Also install ESP32 board support (Espressif) in Board Manager.

## Firmware Configuration

Edit [drift_monitor/drift_monitor.ino](drift_monitor/drift_monitor.ino) and set:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `DATABASE_URL`

Note: This firmware uploads via Firebase Realtime Database REST (`.json` endpoints), so `API_KEY` is not required for open test rules.

If you use DHT11, change:

- `#define DHTTYPE DHT22` to `#define DHTTYPE DHT11`

If your LCD I2C address is different, change:

- `#define LCD_I2C_ADDRESS 0x27` (common alternative: `0x3F`)

## Firebase Realtime Database Rules (for quick testing)

Use this only for development testing:

```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

For production, lock rules down.

## Data Paths Written by ESP32

- `/sensor/temperature`
- `/sensor/humidity`

The web dashboard in [public/index.html](public/index.html) already reads from these paths.

## Upload and Test

1. Connect ESP32 by USB.
2. In Arduino IDE select board: ESP32 Dev Module (or your exact board).
3. Select the correct COM/USB port.
4. Upload [drift_monitor/drift_monitor.ino](drift_monitor/drift_monitor.ino).
5. Open Serial Monitor at 115200 baud.
6. Confirm messages:
   - WiFi connected
   - Firebase ready
   - Temperature/humidity updates every 5 seconds
7. Confirm LCD shows sensor values and sync status.
8. Run `firebase deploy --only hosting` to publish web dashboard.

## If Something Still Fails

- DHT shows read errors:
  - Verify pull-up resistor on DATA line.
  - Verify DHT pin is GPIO4.
  - Try slower polling (already 5 seconds).
  - Confirm DHT type (DHT11 vs DHT22).
- LCD blank:
  - Check I2C address (`0x27` vs `0x3F`).
  - Confirm SDA=21 and SCL=22.
  - Adjust contrast screw on LCD backpack.
- Firebase upload fails:
  - Recheck API key and database URL.
  - Verify RTDB region in URL.
  - Verify Firebase rules allow write for test.
- WiFi unstable:
  - Use 2.4GHz SSID (ESP32 does not use 5GHz-only networks).
