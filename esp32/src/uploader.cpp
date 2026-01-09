#include "uploader.h"
#include "uploader_config.h"
#include <HTTPClient.h>
#include "esp_camera.h"

static void uploaderTask(void *pvParameters) {
  (void) pvParameters;

  HTTPClient http;

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("[uploader] Camera capture failed");
      } else {
        // POST buffer to gateway
        if (http.begin(UPLOAD_URL)) {
          http.addHeader("Content-Type", "application/octet-stream");
          if (strlen(UPLOAD_API_KEY) > 0) {
            http.addHeader("X-API-KEY", UPLOAD_API_KEY);
          }

          int httpCode = http.sendRequest("POST", fb->buf, fb->len);

          if (httpCode > 0) {
            String payload = http.getString();
            Serial.printf("[uploader] POST %d - %s\n", httpCode, payload.c_str());
          } else {
            Serial.printf("[uploader] POST failed, error: %s\n", http.errorToString(httpCode).c_str());
          }

          http.end();
        } else {
          Serial.println("[uploader] http.begin failed");
        }

        esp_camera_fb_return(fb);
      }
    } else {
      Serial.println("[uploader] WiFi not connected, skipping upload");
    }

    vTaskDelay(pdMS_TO_TICKS(UPLOAD_INTERVAL_MS));
  }
}

void startUploaderTask() {
#if UPLOAD_ENABLED
  xTaskCreatePinnedToCore(uploaderTask, "uploader", 8 * 1024, NULL, 1, NULL, 1);
  Serial.println("[uploader] uploader task started");
#else
  Serial.println("[uploader] uploader is disabled (UPLOAD_ENABLED=false)");
#endif
}
