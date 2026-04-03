# 🧭 TrailSense V4: Smart Photo Frame

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Supported-red)
![LVGL](https://img.shields.io/badge/LVGL-v8.x-blue)
![Storage](https://img.shields.io/badge/Storage-FFat%20(12.5MB)-brightgreen)

TrailSense is a custom firmware for the **Waveshare ESP32-S3-Touch-AMOLED-1.43**. It transforms the smartwatch-sized display into a standalone digital photo frame and inclinometer, complete with its own WiFi hotspot, captive portal, and a mobile-friendly Web UI for cropping and syncing photos directly to the device.

---

## ✨ Features
* **📱 Standalone Web UI:** Built-in captive portal. Connect to the watch's WiFi and the upload page pops up automatically.
* **✂️ Live Image Editor:** Pan, zoom, and crop photos directly on your phone before sending them to the watch.
* **💾 FFat Storage:** Utilizes the ESP32's massive 16MB flash memory to store up to ~28 high-res 16-bit images.
* **👆 Touch Gestures:** Swipe to navigate photos, long-press to delete.
* **📷 Hardcoded QR Code:** Zero-RAM QR code rendered on the display for instant WiFi pairing.
* **📐 Inclinometer Ready:** Lightweight architecture leaves plenty of RAM available for real-time sensor animations.

---

## 🛠️ Hardware Requirements
* **Board:** [Waveshare ESP32-S3-Touch-AMOLED-1.43](https://www.waveshare.com/esp32-s3-touch-amoled-1.43.htm)
* **Connectivity:** USB-C Cable for flashing

---

## ⚙️ Arduino IDE Board Settings
To compile this project successfully, you **must** configure the Arduino IDE to unlock the full 16MB of Flash and enable PSRAM. 

Go to **Tools** and set the following parameters:

| Setting | Value |
| :--- | :--- |
| **Board** | `ESP32S3 Dev Module` |
| **USB CDC On Boot** | `Enabled` |
| **Flash Size** | `16MB (128Mb)` |
| **Partition Scheme** | `16M Flash (2MB APP/12.5MB FATFS)` ⚠️ *Crucial for image storage!* |
| **PSRAM** | `OPI PSRAM` |

*Note: If you change the partition scheme, the ESP32 will automatically format the FFat drive on its first boot. This takes about 10-15 seconds. Subsequent boots will be instant.*

---

## 📖 User Guide

### 1. System Architecture & Sync Flow
The device acts as its own router. You do not need an internet connection or a home WiFi network to sync photos.

```mermaid
sequenceDiagram
    participant User as 📱 User (Phone)
    participant ESP32 as ⌚ TrailSense (ESP32)
    participant FFat as 💾 Internal Storage
    
    ESP32->>User: Displays QR Code
    User->>ESP32: Scans QR & Connects to "TrailSense_Sync"
    ESP32->>User: Captive Portal opens http://trailsense.io
    User->>User: Selects, Pans, and Zooms Photo
    User->>ESP32: Taps "Send to Device" (RGB565 Binary)
    ESP32->>FFat: Saves as .bin file
    ESP32->>ESP32: Updates Slideshow
