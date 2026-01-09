# ESP32-S3 Camera Firmware

This folder contains the PlatformIO project for the ESP32-S3 camera module.

## Setup

1. Install [PlatformIO](https://platformio.org/)
2. Open this folder in VS Code with PlatformIO extension
3. Build and upload to your ESP32-S3 board

## Structure

- `src/` - Source code files
- `include/` - Header files
- `platformio.ini` - PlatformIO configuration

## New uploader task (added)

A new optional uploader task is available to POST frames from the ESP32 to the Node.js gateway for inference.

Configuration (edit `src/uploader_config.h`):

- `UPLOAD_ENABLED` - 1 to enable automatic uploads, 0 to disable
- `UPLOAD_INTERVAL_MS` - Upload interval in milliseconds
- `UPLOAD_URL` - Full URL of the gateway `/upload` endpoint (e.g. `http://<pc-ip>:3000/upload`)
- `UPLOAD_API_KEY` - API key header value to include in `X-API-KEY` (leave empty if not used)

How it works:
- The uploader captures a frame (JPEG) using `esp_camera_fb_get()` and sends it as raw bytes with `Content-Type: application/octet-stream` to `UPLOAD_URL`.
- The gateway should forward the frame to the Python AI server and broadcast detection metadata back to clients.

Notes:
- Test first on the same LAN (set `UPLOAD_URL` to your PC LAN IP) - ensure firewall allows port 3000.
- When using ngrok, set `UPLOAD_URL` to the ngrok URL. Keep upload interval and JPEG quality low for reliability.

