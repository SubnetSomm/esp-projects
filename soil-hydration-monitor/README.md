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
- Calibration is editable and persists. The provisional planted-soil calibration uses needs-water=2959 and settled-wet=2574. These are relative reference points, not soil water-content measurements. Original air/water defaults (3039/1370) migrate once, with prior endpoints retained in device Preferences; custom calibration is preserved. Updating calibration rescales displayed history, while saved raw readings remain unchanged.

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

## Plant watering guidance (2026-10-26)

The dashboard now distinguishes Moisture looks okay, Water soon, Checking dry soil, Water your plant, and Check the probe. The needs-water reference is 2959 counts (six pre-watering readings were 2958–2959). The settled-wet reference is 2574 counts (median of 21 late-afternoon readings after the watering day). Both are provisional: the probe was removed/reinserted around mode changes, so soil contact is a source of uncertainty. Refine with ordinary plant care while keeping depth and position consistent.

The relative scale maps the needs-water reference to 0% and the settled-wet reference to 100%. This is not a bone-dry-to-saturated scale. Water soon means 15% or below. Water your plant requires at least three consecutive samples at/below 0%, spanning at least 30 seconds. The alert clears only after three consecutive samples at/above 20%, spanning at least 30 seconds. In 15-minute monitoring this normally takes about 30 minutes; in Watering mode it takes at least 30 seconds. Rapid mode changes cannot satisfy the time requirement. These are provisional decision rules, not species-specific horticultural limits.

Evidence counters survive deep sleep in RTC memory. The confirmed alert persists across complete power loss in Preferences; cold boots restart consecutive evidence. Changing calibration resets the alert/evidence and takes a fresh sample. Near-rail readings (100 counts or below, or 4090 or above) show Check the probe, interrupt consecutive evidence, and cannot clear an existing alert. A probe removed into air can still look like dry soil; firmware cannot reliably distinguish that condition with this hardware.

The dashboard presents recommendations only while connected, and labels them unavailable/stale if the connection fails. There are no background phone notifications. History record layout, raw soil data, CSV columns, and sleep/button behavior are preserved. Battery voltage now has a provisional single-point gain correction from a meter comparison; the historical voltage trend alone cannot calibrate remaining capacity.

Validation: host C++ tests cover confirmation duration, monitoring and watering cadences, threshold boundaries, hysteresis, invalid sensor input, reversed calibration, and RTC-state continuation. Hardware validation remains required after upload.

## Battery voltage calibration (2026-10-26)

The user measured 3.822 V across the battery while the sampled dashboard value was 3.75 V. New ADC-derived battery readings are multiplied by 3822/3750 = 1.0192 before display, percentage estimation, and history encoding. This is a single-point gain adjustment, assuming the supplied readings were simultaneous on battery power with USB disconnected as requested. It is not a full-range ADC calibration or a measured capacity curve. Check another paired reading later at a different charge level.

Previously recorded history and CSV exports remain unchanged; older and newer voltage points therefore use different calibration. New history still uses the existing 20 mV resolution. The percentage remains a generic voltage-based estimate affected by load and charging. Missing/invalid ADC inputs are not converted into plausible voltages.
