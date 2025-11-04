| Supported Operating Systems | Unix | Windows |
| --------------------------- | ---- | ------- |


# Underwater_LIFI
This project is the embedded software communication protocol for establishing communication between two esp32s3 boards via LiFi. The targeted use case is underwater LiFi.

## Setup Build Environment

```
  git submodule init
  git submodule update

  cd esp-idf/
```

* Run the install script to set up the build environment. The options include `install.bat` or `install.ps1` for Windows, and `install.sh` or `install.fish` for Unix shells.
* Run the export script on Windows (`export.bat`) or source it on Unix (`source export.sh`) in **every shell environment before using ESP-IDF.**

## Menu Configurations

```
  cd ..
```
* You should be the int Physical-Layer/ Directory

```
  idf.py set-target esp32s3
  idf.py menuconfig
```
* Inside the menu go to Example Ethernet Configuration -> Enable SPI. Set Interrupt GPIO to pin 9 and set PHY Reset GPIO pin to 8.
* Press `S` to save, then press `Q` to quit. 

## Flashing the Transciever

* Now you can run `idf.py build flash monitor` to build and flash the ESP32.

