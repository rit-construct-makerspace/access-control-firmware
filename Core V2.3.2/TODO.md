## TODO List for Core V2.3.2

### Short-Term:

* check why devices are not returning from white light when reconnecting to network
* Devices not reporting session length in make, see what is different in the reporting now versus before.

### Long-Term:

* Transfer code to ESP-IDF for better secruty and configuration
* Implement messagereport task
* Add switch type detection, component count detection
* OneWire scanning code to detect infinite devices, gather temperature data from all of them
* Occasionally check OneWire to ensure all devices are there
* occasionally check for the same/old ID card UID