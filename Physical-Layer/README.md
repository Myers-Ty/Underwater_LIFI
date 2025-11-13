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
* Inside the menu go to Example Connection Configuration -> Enable SPI. Set Ethernet Type to W5500 Module. Set the following:
* (12)    SPI SCLK GPIO number
* (11)    SPI MOSI GPIO number
* (13)    SPI MISO GPIO number
* (10)    SPI CS GPIO number
* (36)    SPI clock speed (MHz)
* (9)     Interrupt GPIO number
* (8)     PHY Reset GPIO number

* Press `S` to save, then press `Q` to quit. 

## Flashing the Transciever

* Now you can run `idf.py build flash monitor` to build and flash the ESP32.

## Testing the Interface

* Create a venv from the requirements.txt file by running
```
  python3 -m venv venv_for_TestEth
  source venv_for_TestEth/bin/activate

  pip install -r 'requirements.txt'
```

* Then exit the venv and run the test script with the following
```
deactivate
sudo ./venv_for_TestEth/bin/python3  TestEth.py
```

