# KeeLoq IoT Gate Controller

A smart IoT solution for controlling a driveway gate. This project bridges the gap between traditional RF remote controls and modern smartphone automation. 

The primary plan and goal of this project is to maintain the full functionality of existing KeeLoq physical remotes (keyfobs) while simultaneously allowing you to open and close the gate conveniently from your smartphone.

## 📱 App Interface

<p align="center">
  <img src="https://github.com/user-attachments/assets/e6fdc85c-b932-46fb-99dd-7ee1a64c84b0" alt="App Screenshot" width="250">
  <img src="https://github.com/user-attachments/assets/43bda41d-6309-48f6-b90b-3c0d0277c8dc" alt="App Screenshot" width="250">
  <img src="https://github.com/user-attachments/assets/9e6ca7f6-6b2b-49e4-bf68-c0ba01a515d7" alt="App Screenshot" width="250">
</p>

---

## ✨ Features

* **Dual Control System:** Operate your gate using traditional KeeLoq RF remotes or via the mobile application.
* **Adafruit IO Broker:** Utilizes **Adafruit IO** as the MQTT cloud broker for fast and reliable communication between the MCU and the smartphone.
* **Secure Authentication:** Requires specific Adafruit IO credentials for the gateway app to communicate with the gate.

---

## 🚀 Getting Started

To get this project up and running, you need to configure both the Microcontroller (MCU) and the Gateway app before building. 

### 1. MCU Configuration

Before flashing the firmware to your microcontroller, you must configure your local environment settings and provide your Adafruit IO credentials.

1. Navigate to the MCU source directory.
2. Open the `config.h` file.
3. Update the necessary definitions with your Wi-Fi details and Adafruit IO account keys.

```c
constexpr const char* WIFI_SSID     = "";
constexpr const char* WIFI_PASSWORD = "";
constexpr const char* AIO_USERNAME  = "";
constexpr const char* AIO_KEY       = "";
```

### 2. Gateway Application Configuration

The Gateway application requires your specific Adafruit IO authentication credentials to securely connect to the broker and control the gate.

1. Navigate to the gateway project directory.
2. Open `main.dart`.
3. Insert your specific credentials where indicated.

```dart
// Example of what to look for in main.dart
final String _username = "";
final String _apiKey = "";
```

---

## 🛠️ Tech Stack

* **MCU:** C/C++ (Handles KeeLoq rolling code decryption and physical relay control).
* **Cloud:** Adafruit IO (MQTT Broker).
* **Gateway/App:** Dart (Provides the user interface for smartphone control).
