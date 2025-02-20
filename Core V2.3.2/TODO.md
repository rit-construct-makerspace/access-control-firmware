## TODO List for Core V2.3.2

### Core FrontEnd
* Rewrite to remove buggy and weird operation
* Make buzzer go off constantly if key swtich moved without keycard present

### Core Backend
* Add OTA pull based updates
* Add switch type detection, component count detection
* OneWire scanning code to detect infinite devices, gather temperature data from all of them
* Occasionally check OneWire to ensure all devices are there
* occasionally check for the same/old ID card UID
* Re-write code for handling no server connection, no WiFi and checking for them

