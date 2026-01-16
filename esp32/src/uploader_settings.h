#ifndef UPLOADER_SETTINGS_H
#define UPLOADER_SETTINGS_H

#include <Arduino.h>

void uploader_settings_init();
String uploader_get_url();
String uploader_get_api_key();
uint32_t uploader_get_interval_ms();
void uploader_set_url(const char *url);
void uploader_set_api_key(const char *key);
void uploader_set_interval_ms(uint32_t ms);

// Device identifiers and optional public stream URL
String uploader_get_device_id();
String uploader_get_stream_url();
void uploader_set_device_id(const char *id);
void uploader_set_stream_url(const char *url);

// Optional upload capture parameters
int uploader_get_frame_size();
void uploader_set_frame_size(int framesize);
int uploader_get_jpeg_quality();
void uploader_set_jpeg_quality(int q);

// Upload queue settings
bool uploader_is_queue_enabled();
void uploader_set_queue_enabled(bool en);
int uploader_get_queue_size();
void uploader_set_queue_size(int size);

// Gateway host (host or full URL) used to construct upload endpoint
String uploader_get_gateway();
void uploader_set_gateway(const char *gateway);

// Returns true if an explicit uploader URL is saved in preferences
bool uploader_is_configured();

#endif // UPLOADER_SETTINGS_H
