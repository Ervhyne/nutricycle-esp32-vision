#ifndef UPLOADER_CONFIG_H
#define UPLOADER_CONFIG_H

// Include camera definitions so frame size constants (e.g., FRAMESIZE_QQVGA) are available
#include "esp_camera.h"

// Enable/disable automatic uploads from the ESP32
#define UPLOAD_ENABLED 1

// Upload interval in milliseconds (default 2000ms -> ~0.5 FPS)
// Lowering upload frequency helps reliability across slow or unstable tunnels.
#define UPLOAD_INTERVAL_MS 2000

// Default frame size and JPEG quality to reduce bandwidth for uploads
// FRAMESIZE_QQVGA (160x120) is recommended for slow links. Adjust if higher-res is needed.
#define UPLOAD_FRAME_SIZE FRAMESIZE_QQVGA
// JPEG quality: 0 = best quality, 63 = worst. Higher numbers reduce bandwidth. Default: 24
#define UPLOAD_JPEG_QUALITY 24

// URL of the Node gateway upload endpoint
// Replace with your machine's LAN IP or ngrok URL during testing
#define UPLOAD_URL "http://192.168.1.16:3000/upload"

// API key expected by the gateway (if used). Keep empty if not used.
#define UPLOAD_API_KEY "changeme"

// Persistent upload queue (LittleFS) settings
#define UPLOAD_QUEUE_ENABLED 1
#define UPLOAD_QUEUE_SIZE 10  // number of frames to persist

#endif // UPLOADER_CONFIG_H
