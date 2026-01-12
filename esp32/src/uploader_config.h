#ifndef UPLOADER_CONFIG_H
#define UPLOADER_CONFIG_H

// Enable/disable automatic uploads from the ESP32
#define UPLOAD_ENABLED 1

// Upload interval in milliseconds (default 500ms -> ~2 FPS)
#define UPLOAD_INTERVAL_MS 500

// URL of the Node gateway upload endpoint
// Replace with your machine's LAN IP or ngrok URL during testing
#define UPLOAD_URL "http://192.168.1.16:3000/upload"

// API key expected by the gateway (if used). Keep empty if not used.
#define UPLOAD_API_KEY "changeme"

#endif // UPLOADER_CONFIG_H
