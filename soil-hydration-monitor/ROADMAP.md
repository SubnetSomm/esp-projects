# Planned iterations

## Wi-Fi connection settings
Add portal controls for Host, Join, and Host + Join modes. Prefer Host + Join for portable demonstrations, save network settings, show the joined network address, and keep hotspot access available when the saved network is unavailable. Deep sleep turns off both Wi-Fi connections. Not implemented yet.

## Plant-specific watering guidance
Use the current Watering mode to collect stable raw readings for approximately one minute before watering a plant known to need water. Water normally, allow excess water to drain, then collect stable readings again. Keep probe placement consistent. Use these as provisional needs-water and well-watered references, rather than measured volumetric water content.

Provisional needs-water (2959) and settled-wet (2574) references and dashboard guidance are now implemented, with consecutive-reading confirmation and a separate clearing threshold. Physical validation and battery reference measurements remain pending. Add watering-event markers in a later iteration. Refine through ordinary use; a forced complete drying cycle is not required.

## Product validation
Verify physical button sleep/wake, timer logging and timestamps, retained history after sleep and power interruption, battery-only runtime, battery voltage against a meter, and practical enclosure/wiring protection. Background iPhone alerts need a separate receiving service or device; the current local webpage does not provide them.
