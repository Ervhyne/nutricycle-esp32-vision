#include "uploader.h"
#include "uploader_config.h"
#include "uploader_settings.h"
#include <HTTPClient.h>
#include "esp_camera.h"

static void uploaderTask(void *pvParameters) {
  (void) pvParameters;

  HTTPClient http;

  // Initialize settings (Preferences)
  uploader_settings_init();

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("[uploader] Camera capture failed");
      } else {
        String uploadUrl = uploader_get_url();
        String apiKey = uploader_get_api_key();
        uint32_t interval = uploader_get_interval_ms();

        // If upload URL is not configured, try to build it from stored gateway host
        if (uploadUrl.length() == 0) {
          String gw = uploader_get_gateway();
          if (gw.length() > 0) {
            if (!gw.startsWith("http://") && !gw.startsWith("https://")) gw = String("http://") + gw;
            if (gw.endsWith("/")) gw = gw.substring(0, gw.length()-1);
            uploadUrl = gw + "/upload";
          }
        }

        // If upload URL is still not configured, skip upload and keep AP available for provisioning
        if (uploadUrl.length() == 0) {
          Serial.println("[uploader] upload URL not configured, skipping upload");
          esp_camera_fb_return(fb);
          vTaskDelay(pdMS_TO_TICKS(interval));
          continue;
        }

        // POST buffer to gateway
        if (http.begin(uploadUrl.c_str())) {
          http.addHeader("Content-Type", "application/octet-stream");
          if (apiKey.length() > 0) {
            http.addHeader("X-API-KEY", apiKey.c_str());
          }
          // Include optional headers to help Node register or resolve the device stream
          String streamUrl = uploader_get_stream_url();
          String deviceId = uploader_get_device_id();
          if (streamUrl.length() > 0) http.addHeader("X-STREAM-URL", streamUrl.c_str());
          if (deviceId.length() > 0) http.addHeader("X-DEVICE-ID", deviceId.c_str());

          int httpCode = http.sendRequest("POST", fb->buf, fb->len);

          static bool stream_registered = false;
          if (httpCode > 0) {
            String payload = http.getString();
            Serial.printf("[uploader] POST %d -> %s\n", httpCode, uploadUrl.c_str());
            Serial.printf("[uploader] Response: %s\n", payload.c_str());

            // Attempt to register public stream URL once (if available and gateway exists)
            if (!stream_registered) {
              String gw = uploader_get_gateway();
              String sUrl = uploader_get_stream_url();
              String devId = uploader_get_device_id();
              String apiKeyLocal = uploader_get_api_key();
              if (gw.length() > 0 && sUrl.length() > 0 && devId.length() > 0) {
                if (!gw.startsWith("http://") && !gw.startsWith("https://")) gw = String("http://") + gw;
                if (gw.endsWith("/")) gw = gw.substring(0, gw.length()-1);
                String regUrl = gw + "/devices/" + devId + "/register_stream";
                HTTPClient rh;
                rh.begin(regUrl.c_str());
                rh.addHeader("Content-Type", "application/json");
                if (apiKeyLocal.length() > 0) rh.addHeader("X-API-KEY", apiKeyLocal.c_str());
                String body = String("{\"url\":\"") + sUrl + String("\"}");
                int rc = rh.POST((uint8_t*)body.c_str(), body.length());
                if (rc > 0 && (rc >= 200 && rc < 300)) {
                  Serial.printf("[uploader] Stream registered (%d) -> %s\n", rc, regUrl.c_str());
                  stream_registered = true;
                } else {
                  Serial.printf("[uploader] Stream register failed (%d) -> %s\n", rc, regUrl.c_str());
                }
                rh.end();
              }
            }

          } else {
            Serial.printf("[uploader] POST failed (%d) -> %s\n", httpCode, uploadUrl.c_str());
          }

          http.end();
        } else {
          Serial.printf("[uploader] http.begin failed for %s\n", uploadUrl.c_str());
        }

        esp_camera_fb_return(fb);

        // Use dynamic interval in case user updated settings via web UI
        vTaskDelay(pdMS_TO_TICKS(interval));
        continue; // Skip the static delay at bottom
      }
    } else {
      Serial.println("[uploader] WiFi not connected, skipping upload");
    }

    // If not connected use default interval
    vTaskDelay(pdMS_TO_TICKS(uploader_get_interval_ms()));
  }
}

static bool uploader_started = false;

void startUploaderTask() {
#if UPLOAD_ENABLED
  if (uploader_started) {
    Serial.println("[uploader] uploader task already started");
    return;
  }
  xTaskCreatePinnedToCore(uploaderTask, "uploader", 8 * 1024, NULL, 1, NULL, 1);
  uploader_started = true;
  Serial.println("[uploader] uploader task started");
#else
  Serial.println("[uploader] uploader is disabled (UPLOAD_ENABLED=false)");
#endif
}
