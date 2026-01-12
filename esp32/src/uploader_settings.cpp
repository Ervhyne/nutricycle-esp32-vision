#include "uploader_settings.h"
#include <Preferences.h>
#include "uploader_config.h"
#include <WiFi.h>

static Preferences prefs;
static const char *NS = "uploader";

void uploader_settings_init() {
  prefs.begin(NS, false);
}

String uploader_get_url() {
  // Return empty if not explicitly set, so the firmware can detect 'not configured'
  String s = prefs.getString("url", String(""));
  return s;
}

String uploader_get_api_key() {
  String s = prefs.getString("api", String(""));
  return s;
}

uint32_t uploader_get_interval_ms() {
  uint32_t v = prefs.getUInt("interval", UPLOAD_INTERVAL_MS);
  return v;
}

// Device ID stored or default to MAC address
String uploader_get_device_id() {
  String s = prefs.getString("device_id", String(""));
  if (s.length() == 0) {
    byte mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[32];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
  }
  return s;
}

String uploader_get_stream_url() {
  String s = prefs.getString("stream_url", String(""));
  return s;
}

bool uploader_is_configured() {
  // Consider uploader configured if gateway or url is set
  String gw = prefs.getString("gateway", String(""));
  if (gw.length() > 0) return true;
  
  String url = prefs.getString("url", String(""));
  return url.length() > 0;
}

void uploader_set_url(const char *url) {
  if (url && strlen(url) > 0) prefs.putString("url", String(url));
}

void uploader_set_api_key(const char *key) {
  if (key) prefs.putString("api", String(key));
}

void uploader_set_interval_ms(uint32_t ms) {
  if (ms < 100) ms = 100; // clamp
  prefs.putUInt("interval", ms);
}

void uploader_set_device_id(const char *id) {
  if (id && strlen(id) > 0) prefs.putString("device_id", String(id));
}

void uploader_set_stream_url(const char *url) {
  if (url && strlen(url) > 0) prefs.putString("stream_url", String(url));
}

String uploader_get_gateway() {
  String s = prefs.getString("gateway", String(""));
  return s;
}

void uploader_set_gateway(const char *gateway) {
  if (gateway && strlen(gateway) > 0) prefs.putString("gateway", String(gateway));
} 
