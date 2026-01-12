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

// Gateway host (host or full URL) used to construct upload endpoint
String uploader_get_gateway();
void uploader_set_gateway(const char *gateway);

// Returns true if an explicit uploader URL is saved in preferences
bool uploader_is_configured();

#endif // UPLOADER_SETTINGS_H
