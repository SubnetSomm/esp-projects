# Standalone soil monitor — local Wi-Fi dashboard

This version runs on the XIAO ESP32C3 without Home Assistant, a router, internet access, or a phone app. The phone connects directly to the device Wi-Fi and opens its embedded webpage.

## Connect

1. Join **Soil Monitor** Wi-Fi. A device-specific password is generated on its first standalone boot and retained in Preferences.
2. Read the password in Serial Monitor at 115200 baud. Send `?` to print it again.
3. Open **http://192.168.4.1** in Safari. Stay connected if iOS says the network has no internet.
4. Optionally use Safari Share > Add to Home Screen as a shortcut. This does not make the dashboard accessible when disconnected from the device network.

## Behavior

- Monitor: record every 15 minutes.
- Watering: record every 5 seconds, then automatically return to Monitor after 10 minutes.
- Switching modes records an immediate sample. Reboot defaults to Monitor.
- Wi-Fi remains active while interacting with the dashboard. Hold the D2 button for 2 seconds and release to save a final reading and enter deep sleep. Timer wakes log every 15 minutes without starting Wi-Fi, then sleep again. A button press wakes the hotspot; reconnect the iPhone to view history. The waking press is ignored until released. Reset/power-on starts the hotspot. Sleep is cancelled if storage is unhealthy. Runtime has not been measured; the soil probe remains powered.
- View 24-hour, 7-day, or all timestamped history; tap the chart to inspect a reading.
- Download raw and calibrated readings through Export CSV.
- Calibration is editable and persists. Initial dry=3039 and wet=1370 values are preliminary dry/water endpoints, not soil water-content measurements. Updating calibration rescales displayed history, while saved raw readings remain unchanged.

## History and clock

LittleFS stores a bounded binary log independently of the phone. Two rotating files hold between roughly 4096 and 8192 records in total (64–128 KiB). When a new file rotates in, the oldest full file is removed. Watering samples use the same storage as monitoring samples. Export periodically if you need permanent retention. Sudden power loss may lose the newest write; the history store is not a laboratory data recorder.

The phone supplies date/time when opening the page. The device uses elapsed time to continue its clock while powered. After complete power loss, opening the dashboard anchors the new boot session, including samples already captured in that session. Up to 32 boot-time anchors are retained. Readings without a retained anchor remain in CSV with their session ID and elapsed seconds; they are excluded from calendar-based charts rather than assigned invented dates. There is no external RTC.

## Hardware

- Seeed XIAO ESP32C3, soil signal A0/GPIO2, sensor power 3V3 and GND.
- PKCELL LP803860 3.7 V 2000 mAh battery, with existing USB charging hardware.
- Battery measurement: BAT+ → 100KΩ → A1/GPIO3 → 100KΩ → GND. Averaged calibrated ADC voltage is doubled for the divider. Button: D2/A2/GPIO4 pulled up to 3V3 through external 10KΩ; normally open momentary switch between GPIO4 and GND. No indicator LED, screen, Bluetooth, or Home Assistant integration is implemented. Deep sleep retains the session clock and boot ID in RTC memory so phone clock anchors continue across timer wakes; power loss starts a new session. Existing 16-byte history records and CSV format are preserved.

## Build and upload

From this folder:

```sh
pio run
pio run -t upload --upload-port /dev/cu.usbmodem2101
pio device monitor --port /dev/cu.usbmodem2101 --baud 115200
```

The USB port can change. The previous ESPHome firmware may hide the USB port while asleep; press Reset to wake it before uploading. No separate filesystem upload is needed: the webpage is embedded in firmware. The log filesystem is initialized on first standalone boot. Subsequent filesystem failures are reported instead of automatically formatting saved history.

The Wi-Fi password is unique to the device, not hardcoded in source. Dashboard changes require a same-origin token in addition to joining the hotspot. Anyone with hotspot access can view/export history. There is no internet-facing service.

## Validation / current state

- PlatformIO compilation passed for the XIAO ESP32C3.
- Dashboard checked at iPhone-sized viewport with sample data; monitoring/watering controls, calibration form, chart layout, and page loading checked.
- Standalone firmware successfully uploaded to /dev/cu.usbmodem2101; all flash hashes verified. Normal boot and the Soil Monitor hotspot setup output were confirmed over USB; history storage reports healthy with the first saved record.
- Wi-Fi page controls, retention across a power cycle, and battery-only operation still require verification from the connected phone.

The prior Home Assistant project remains in ../SoilHydrationMonitor and a verified source/firmware/credential archive is in ../FirmwareArchives. Keep the archive private because it contains device keys.

## Battery recording (2026-10-21)

Battery voltage and an approximate single-cell LiPo percentage are captured with every soil sample (15-minute monitoring / 5-second watering). The dashboard shows the latest sampled voltage/estimate and has a Battery voltage history view. CSV adds battery_voltage and battery_estimated_percent columns. USB charging and load affect the percentage; it is not a charging-status detector or calibrated fuel gauge. Check voltage against a multimeter on battery-only power, especially with the 100K divider and no filter capacitor.

The original 16-byte history format is preserved. Its formerly unused byte now stores voltage in 20 mV steps; legacy zero bytes are treated as missing battery data, not zero volts. Status readings retain millivolt resolution. Values outside 2.5–4.5 V are marked unavailable. Existing Wi-Fi credentials, calibration, and history are retained.

Battery conversion/quantization and monotonic percentage tests passed; firmware compilation and dashboard JavaScript syntax passed. Browser preview was unavailable during this update. If OneDrive causes build timeouts, set PLATFORMIO_BUILD_DIR to a local temporary folder when invoking PlatformIO.

Battery firmware uploaded and flash-verified on 2026-10-21. USB verification reported 4.134 V (97% estimate) and healthy history with 1554 current records after upgrade (1553 before). Dashboard logic checks passed for voltage formatting, invalid/missing battery values, and excluding legacy records from the voltage chart. Browser visual verification and multimeter calibration remain pending.
