e# MLX90642 ESP32 Web Interface

This project demonstrates how to interface with the Melexis MLX90642 far-infrared thermal sensor array using an ESP32 microcontroller. The data from the sensor is displayed on a web interface served by the ESP32.

## Sensor Reference
- [MLX90642 Far-Infrared Thermal Sensor Array High SNR](https://www.melexis.com/en/product/mlx90642/far-infrared-thermal-sensor-array-high-snr)

## Features
- Connects to the MLX90642 sensor via I2C.
- Serves a web interface over Wi-Fi in Access Point (AP) mode.
- Displays thermal data on a web page with color mapping.

## Dependencies
- **ESP-IDF** - Espressif IoT Development Framework version 5.0 or later.
- **Espressif Toolchain** - GCC toolchain for ESP32 development.
- **I2C Driver** - Built-in I2C driver in ESP-IDF.

## Folder Structure

MLX90642_ESP32_Web_Interface/
├── build/
├── main/
│   ├── CMakeLists.txt
│   └── MLX90642_ESP32_Web_Interface.c
├── components/
├── CMakeLists.txt
└── README.md


## Installation Instructions

### 1. Install ESP-IDF and Espressif Toolchain
- **Download and install the ESP-IDF** from [Espressif's official website](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).
- Follow the installation instructions specific to your operating system.

### 2. Set Up Environment Variables
- Open a terminal or command prompt.
- Navigate to the ESP-IDF directory and run the `export.sh` script:
    sh
cd /path/to/esp-idf
source export.sh


### 3. Clone this Repository
- Clone the repository to your local machine:
    sh
git clone https://github.com/yourusername/MLX90642_ESP32_Web_Interface.git
cd MLX90642_ESP32_Web_Interface


### 4. Configure the Project
- Run the `idf.py menuconfig` command to configure your project:
    sh
idf.py menuconfig

- Set the Wi-Fi SSID and password in `Example Configuration` -> `WiFi SSID` and `WiFi Password`.

### 5. Build the Project
- Build the project using the following command:
    sh
idf.py build


### 6. Flash the Firmware to ESP32
- Connect your ESP32 development board via USB.
- Flash the firmware using the following command:
    sh
idf.py flash

- Monitor the output using the `idf.py monitor` command:
    sh
idf.py monitor


### 7. Access the Web Interface
- Once the ESP32 is running, it will create an Access Point with the SSID and password you configured.
- Connect to this Wi-Fi network from your device.
- Open a web browser and navigate to `http://192.168.4.1` (or the IP address displayed in the serial monitor).
- You should see the thermal data displayed on the web interface.

## Necessary Files
- **MLX90642_ESP32_Web_Interface.c** - Main source file containing the I2C communication and web server code.
- **CMakeLists.txt** - CMake configuration files for building the project.
- **README.md** - This documentation file.

## Additional Information
- The project uses a mutex to handle concurrent access to the shared buffer used by the I2C read task and the web server data handler.
- The sensor data is color-mapped on the web page using JavaScript, providing a visual representation of thermal readings.

Feel free to contribute or modify this project according to your needs. For more detailed information, refer to the [MLX90642 datasheet](https://www.melexis.com/en/product/mlx90642/far-infrared-thermal-sensor-array-high-snr).
