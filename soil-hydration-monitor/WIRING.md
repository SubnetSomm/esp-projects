# Wiring and status LED

Use a regular single-color, two-leg LED for this version. A four-leg RGB LED usually has three color leads and one common lead, but common-anode/common-cathode and pin order must be identified before wiring it. RGB support is not implemented.

## Install the LED

Disconnect USB and the battery before changing wiring.

```text
XIAO D6 / TX / GPIO21 ── [1 kΩ resistor] ── LED anode (+)
                                            LED cathode (−) ── GND
```

1. Connect D6 (also marked TX; GPIO21) to one end of a 1 kΩ resistor.
2. Connect the other resistor end to the LED anode, usually its longer leg.
3. Connect the cathode, usually the shorter leg beside the flat edge of the LED body, to the shared GND. If the legs have been trimmed or markings are unclear, identify polarity with a meter's diode mode.
4. Insulate exposed connections and reconnect power. No additional 3V3 branch is needed for the LED.

The resistor must be in series. The GPIO controls the LED directly; do not connect it across 3V3 and GND. The built-in charging LED is separate and retains its existing charging behavior. D6 cannot simultaneously serve an external UART TX connection; this project uses USB Serial instead. GPIO21 can briefly light the LED during reset/boot before firmware takes control.

## Meaning of the patterns

| Pattern | Meaning while awake |
| --- | --- |
| One brief pulse every 5 seconds | Monitoring; using the portable hotspot |
| Two brief pulses every 5 seconds | Monitoring; joined to the saved Wi-Fi |
| Three brief pulses every 5 seconds | Confirmed watering recommendation |
| One brief flash each second | Watering mode |
| Rapid flashing (5 per second) | Attempting to join saved Wi-Fi |
| Long flash every 2 seconds | Check the probe or history storage on the dashboard |
| Steady after holding the button | Release the button to save and sleep |
| Off | Deep sleep, or no power |

Priority is sleep request, sensor/storage fault, Wi-Fi connection attempt, Watering mode, watering recommendation, then monitoring. Read the dashboard for simultaneous statuses and battery estimate. Timer logging wakes keep the status LED off in firmware; a brief hardware boot flash is possible. A sleeping device does not blink to advertise a watering alert. No LED is needed for the other firmware features to work.

## Existing connections

| Connection | XIAO pin |
| --- | --- |
| Soil sensor signal | A0 / D0 / GPIO2 |
| Soil sensor power | 3V3 and GND |
| Battery divider midpoint | A1 / D1 / GPIO3 |
| Momentary button signal | A2 / D2 / GPIO4 |
| New LED through 1 kΩ | D6 / TX / GPIO21 |

Button: external 10 kΩ pull-up from GPIO4 to 3V3; normally open switch from GPIO4 to GND. A standard four-leg tactile switch has two internally connected pairs; use one leg from each pair that connects only when pressed.

Battery divider: BAT+ → 100 kΩ → A1 → 100 kΩ → GND. Battery remains connected to its existing battery pads; charging remains handled by the board.

See [Seeed's official XIAO ESP32C3 pinout](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) for board pin locations.
