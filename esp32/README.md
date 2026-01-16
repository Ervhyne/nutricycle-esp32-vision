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
- `UPLOAD_INTERVAL_MS` - Upload interval in milliseconds (default 2000ms to improve reliability over tunnels)
- `UPLOAD_FRAME_SIZE` - Default frame size for uploads (e.g., `FRAMESIZE_QQVGA` to reduce bandwidth)
- `UPLOAD_JPEG_QUALITY` - JPEG quality for uploads (0 = best, 63 = worst; higher numbers reduce payload size)
- `UPLOAD_URL` - Full URL of the gateway `/upload` endpoint (e.g. `http://<pc-ip>:3000/upload`)
- `UPLOAD_API_KEY` - API key header value to include in `X-API-KEY` (leave empty if not used)

How it works:
- The uploader captures a frame (JPEG) using `esp_camera_fb_get()` and sends it as raw bytes with `Content-Type: application/octet-stream` to `UPLOAD_URL`.
- The gateway should forward the frame to the Python AI server and broadcast detection metadata back to clients.

Provisioning & runtime configuration:
- If the device cannot connect to Wi‑Fi it will start a SoftAP (`NutriCycle-Setup`) and host a setup page at `http://192.168.4.1/setup` where you can set Wi‑Fi credentials and uploader settings (URL, API key, interval). Settings persist in flash and do not require reflashing.
- The uploader uses `UPLOAD_URL` at runtime; to change between local LAN and ngrok you can update the uploader URL on the device via the setup page or by re-flashing `uploader_config.h`.

Notes:
- Test first on the same LAN (set `UPLOAD_URL` to your PC LAN IP) - ensure firewall allows port 3000.
- When using ngrok, set `UPLOAD_URL` to the ngrok URL. Keep upload interval and JPEG quality low for reliability.

