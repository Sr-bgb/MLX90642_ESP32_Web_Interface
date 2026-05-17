### 1. Update Introduction and Project Overview
- **Description**: Ensure the project overview clearly describes the purpose of the application.
- **Details**: Mention that MLX90642 is a high-precision 32x24 pixel infrared camera from Melexis with low noise, fine resolution, and straightforward integration.

### 2. List Features
- **Web-based Interface**: View thermal camera data in real-time through a web browser.
- **WiFi Access Point**: The ESP32 creates a WiFi access point for easy connection.
- **Data Visualization**: Real-time thermal data visualization with adjustable color palettes and markers.
- **Rate Control**: Set the data refresh rate via a web interface.

### 3. Document Prerequisites
- **ESP-IDF**: Ensure you have the latest version of the ESP-IDF framework installed.
- **Toolchain**: Install the appropriate toolchain for ESP32 development.
- **Git**: For cloning the repository.

### 4. Provide Instructions to Set Up ESP-IDF
- **Step-by-step guide**: Add a link or detailed instructions on setting up the ESP-IDF environment based on the [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).

### 5. Describe Building and Flashing Process
- **Build Command**:
  ```sh
  idf.py build
  ```
- **Flash Command**:
  ```sh
  idf.py -p /dev/ttyUSB0 flash
  ```
- **Monitor Output**:
  ```sh
  idf.py monitor
  ```

### 6. Provide Getting Started Instructions
1. **Clone the Repository**:
   ```sh
   git clone https://github.com/yourusername/MLX90642_ESP32_Web_Interface.git
   cd MLX90642_ESP32_Web_Interface
   ```
2. **Configure WiFi Credentials**:
   - Open `main/include/config.h` and set the desired SSID and password for the WiFi access point.
3. **Build and Flash the Project**:
   ```sh
   idf.py build flash monitor
   ```

### 7. Access the Web Interface
- **Connect to WiFi Network**: Connect to the ESP32's WiFi network (SSID: `MLX90642_ESP32`) with a password of your choice.
- **Open Browser**: Navigate to `http://192.168.4.1` in a web browser.

### 8. Include Folder Structure
- **Main Directory**:
  - `main/`: Contains the main source files and headers.
    - `include/`: Header files.
    - `src/`: Source files.
  - `components/`: Custom components if any.
  - `html/`: Web interface HTML, CSS, JS files.
  - `lib/`: External libraries if used.

### 9. List Dependencies
- **ESP-IDF Framework**:
- **MLX90642 Library**: Include any specific library or driver required for MLX90642 interaction.
- **Web Server Libraries**: Any web server framework used (e.g., ESPAsyncWebServer).

### 10. Document Contributing Guidelines
- **Link to CONTRIBUTING.md**: Add a link to the `CONTRIBUTING.md` file with details on how to contribute to the project.

### 11. Mention License Information
- **License Type**: This project is licensed under the GNU General Public License version 3.
- **Link to LICENSE File**: Add a link to the `LICENSE` file for more details.

### Final Updated README.md Template

```markdown
# MLX90642 ESP32 Web Interface

MLX90642 е високоточен 32x24 пикселен инфрачервен камерă от Melexis. Тя притежава изключително нисък собствен шум, и най-добрата точност между FIR сензорите на момента на пазара. Изключително лесно се работи с нея. Благодарение на точната фабрична калибрация, директно се получава температурата на обекта през I2C интерфейса в градуси. Стойността на регистрите на пикселите се дели на 50, което дава тежест на едно LSB 20 мили келвина.

## Features

- **Web-based Interface**: View thermal camera data in real-time through a web browser.
- **WiFi Access Point**: The ESP32 creates a WiFi access point for easy connection.
- **Data Visualization**: Real-time thermal data visualization with adjustable color palettes and markers.
- **Rate Control**: Set the data refresh rate via a web interface.

## Prerequisites

- **ESP-IDF**: Ensure you have the latest version of the ESP-IDF framework installed. You can download it from [https://github.com/espressif/esp-idf](https://github.com/espressif/esp-idf).
- **Toolchain**: Install the appropriate toolchain for ESP32 development.
- **Git**: For cloning the repository.

## Set Up ESP-IDF
Follow the instructions on the [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) to set up the development environment.

## Build and Flash the Project

1. **Build**:
   ```sh
   idf.py build
   ```
2. **Flash**: Replace `/dev/ttyUSB0` with your device's serial port.
   ```sh
   idf.py -p /dev/ttyUSB0 flash
   ```
3. **Monitor Output**:
   ```sh
   idf.py monitor
   ```

## Getting Started

1. **Clone the Repository**:
   ```sh
   git clone https://github.com/yourusername/MLX90642_ESP32_Web_Interface.git
   cd MLX90642_ESP32_Web_Interface
   ```
2. **Configure WiFi Credentials**: Open `main/include/config.h` and set the desired SSID and password for the WiFi access point.
3. **Build and Flash the Project**:
   ```sh
   idf.py build flash monitor
   ```

## Access the Web Interface

1. Connect to the ESP32's WiFi network (SSID: `MLX90642_ESP32`) with a password of your choice.
2. Open a web browser and navigate to `http://192.168.4.1` to access the thermal camera data.

## Folder Structure

- **Main Directory**:
  - `main/`: Contains the main source files and headers.
    - `include/`: Header files.
    - `src/`: Source files.
  - `components/`: Custom components if any.
  - `html/`: Web interface HTML, CSS, JS files.
  - `lib/`: External libraries if used.

## Dependencies

- **ESP-IDF Framework**
- **MLX90642 Library**
- **Web Server Libraries**

## Contributing

Contributions are welcome! Please read our [CONTRIBUTING.md](CONTRIBUTING.md) file for details on how to contribute to this project.

## License

This project is licensed under the GNU General Public License version 3 - see the [LICENSE](LICENSE) file for details.
```


