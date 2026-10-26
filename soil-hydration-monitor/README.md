# Standalone soil monitor — local Wi-Fi dashboard

This version runs on the XIAO ESP32C3 without Home Assistant or a phone app. Use its portable hotspot, join an existing 2.4 GHz Wi-Fi network, or use both. A router and internet are optional.

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
- Battery measurement: BAT+ → 100KΩ → A1/GPIO3 → 100KΩ → GND. Averaged calibrated ADC voltage is doubled for the divider. Button: D2/A2/GPIO4 pulled up to 3V3 through external 10KΩ; normally open momentary switch between GPIO4 and GND. A single-color status LED connects to D6/GPIO21 through 1 kΩ, with its cathode to GND; see [WIRING.md](WIRING.md). No screen, Bluetooth, or Home Assistant integration is implemented. Deep sleep retains the session clock and boot ID in RTC memory so phone clock anchors continue across timer wakes; power loss starts a new session. Existing 16-byte history records and CSV format are preserved.

## Build and upload

From this folder:

```sh
pio run
pio run -t upload --upload-port /dev/cu.usbmodem2101
pio device monitor --port /dev/cu.usbmodem2101 --baud 115200
```

The USB port can change. The previous ESPHome firmware may hide the USB port while asleep; press Reset to wake it before uploading. No separate filesystem upload is needed: the webpage is embedded in firmware. The log filesystem is initialized on first standalone boot. Subsequent filesystem failures are reported instead of automatically formatting saved history.

The Wi-Fi password is unique to the device, not hardcoded in source. Dashboard changes require a same-origin token in addition to joining the hotspot. Anyone on the hotspot or joined local network can view/export history and obtain the dashboard token used for controls. Wi-Fi credentials are saved in device Preferences, never returned by API responses or included in the repository. The local webpage uses HTTP, not HTTPS. Do not expose it through port forwarding; there is no cloud service.

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

## Wi-Fi settings and LED (2026-10-26)

Open **Wi-Fi & status LED** in the dashboard. Choose Host, Host + Join (recommended), or Join. Find networks or type a 2.4 GHz SSID, enter its password, and save. Only ordinary personal Wi-Fi (8–63 byte passphrase) and explicitly selected open networks are supported; enterprise login and captive-portal sign-in are not implemented. A blank password keeps the saved password only for the same SSID; changing to an open network requires the Open network checkbox. The initial mode remains Host, so existing hotspot access is retained on upgrade.

- Host: broadcasts Soil Monitor as before. Existing joined-network settings are retained for later use.
- Host + Join: keeps the hotspot while also joining the saved network. Connect your iPhone to that existing network to retain normal internet and reach the device's joined IP. The device does not route internet from its joined network to hotspot clients.
- Join: starts with a fallback hotspot. After successfully joining, keeps the hotspot for 120 seconds, shows the joined IP and a device-specific `.local` name, then disables the AP radio. The hotspot returns if the station connection drops. A failed join never removes fallback hotspot access; reconnect attempts recur at roughly one-minute intervals.

Settings apply about one second after the response, without rebooting or clearing history. Radio channel changes or scanning may briefly interrupt a connected phone. If the saved network uses the default hotspot subnet, the hotspot selects a different subnet (normally 192.168.5.1 instead of 192.168.4.1); reconnect and use the address shown in settings or the USB `?` status output. The normal default hotspot address remains 192.168.4.1. Hostnames are device-specific; the IP link remains available if multicast naming is blocked. Guest/client-isolated Wi-Fi can prevent local access even when the device joins.

Deep sleep turns off both Wi-Fi interfaces and the LED. A button wake or Reset starts the saved mode with a fresh hotspot recovery window; unattended timer wakes log and return to sleep without starting Wi-Fi. Networking settings are saved as one versioned Preferences blob; old firmware credentials, calibration, and LittleFS logs remain intact.

LED wiring and blink patterns are in [WIRING.md](WIRING.md) and the portal. The single LED is optional; no pin rewiring of the existing soil sensor, battery divider, or button is needed. Four-leg RGB support is deferred.

Validation: firmware compilation, soil and battery regression tests, LED pulse and network-setting validation tests, and host tests of the real network manager covering failure, retries, two-minute Join grace, actual AP-radio disable, fallback restoration, subnet collision, scanning, and secret-free JSON passed. Dashboard syntax and mocked rendering passed for mode controls, address links, fallback/countdown, preserving edited forms, and handling network names as text. Browser visual verification was unavailable. Real router joins, RF reconnect behavior, and physical LED/sleep patterns require hardware checks.

Combined Wi-Fi/LED/soil-guidance/battery-calibration firmware uploaded on 2026-10-26; all flash hashes verified. USB status confirmed default Host mode, hotspot 192.168.4.1, hostname soil-monitor-6e27ac.local for this device, LED state hotspot_monitor, and healthy history increasing from 2299 to 2300 records across the upgrade. Battery reported 3.954 V on USB power; this is not an independent verification of the battery-only correction because charging was active. Router joins and physical LED wiring remain to be checked.
