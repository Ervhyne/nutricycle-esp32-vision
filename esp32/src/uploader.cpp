#include "uploader.h"
#include "uploader_config.h"
#include "uploader_settings.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include "esp_camera.h"

static void uploaderTask(void *pvParameters) {
  (void) pvParameters;

  HTTPClient http;

  // Initialize settings (Preferences)
  uploader_settings_init();

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      // Ensure camera capture parameters optimized for uploads (may be changed via web UI)
      sensor_t *s = esp_camera_sensor_get();
      int targetFrame = uploader_get_frame_size();
      int targetQuality = uploader_get_jpeg_quality();
      if (s) {
        if (s->pixformat == PIXFORMAT_JPEG) {
          if (s->status.framesize != targetFrame) {
            s->set_framesize(s, (framesize_t)targetFrame);
            Serial.printf("[uploader] adjusted framesize to %d for upload\n", targetFrame);
          }
          s->set_quality(s, targetQuality);
          Serial.printf("[uploader] set jpeg quality to %d for upload\n", targetQuality);
        }
      }

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
            // Special debug/test token: POST to httpbin.org to verify TLS from the device
            if (gw == String("TEST_HTTPBIN")) {
              uploadUrl = String("https://httpbin.org/post");
              Serial.println("[uploader][test] using TEST_HTTPBIN -> https://httpbin.org/post");
            } else {
              if (!gw.startsWith("http://") && !gw.startsWith("https://")) gw = String("http://") + gw;
              if (gw.endsWith("/")) gw = gw.substring(0, gw.length()-1);
              uploadUrl = gw + "/upload";
            }
          }
        }

        // If upload URL is still not configured, skip upload and keep AP available for provisioning
        if (uploadUrl.length() == 0) {
          Serial.println("[uploader] upload URL not configured, skipping upload");
          esp_camera_fb_return(fb);
          vTaskDelay(pdMS_TO_TICKS(interval));
          continue;
        }

        // Queue initialization - LittleFS
  if (uploader_is_queue_enabled()) {
    if (!LittleFS.begin()) {
      Serial.println("[uploader][queue] LittleFS.begin() failed, attempting format...");
      // Try to salvage by formatting once (this will erase any queued frames)
      if (LittleFS.format()) {
        Serial.println("[uploader][queue] LittleFS formatted, attempting mount...");
        if (LittleFS.begin()) {
          Serial.println("[uploader][queue] LittleFS mounted after format");
        } else {
          Serial.println("[uploader][queue] LittleFS.begin() still failed after format");
        }
      } else {
        Serial.println("[uploader][queue] LittleFS.format() failed");
      }
    } else {
      Serial.println("[uploader][queue] LittleFS ready");
      // ensure directory exists
      if (!LittleFS.exists("/uploadq")) {
        LittleFS.mkdir("/uploadq");
      }
    }
  }

  // POST buffer to gateway (with DNS check, TLS support and retries)
  {
    const int maxAttempts = 5; // longer for remote unreliable networks
    int attempt = 0;
    int backoffMs = 1000;
    bool uploaded = false;

    // Persistent secure client to reduce TLS handshake overhead across attempts
    static WiFiClientSecure persistentSecureClient;
    static bool persistentSecureInited = false;

    String host = uploadUrl;
    host.replace("https://", ""); host.replace("http://", "");
    int idx = host.indexOf('/'); if (idx >= 0) host = host.substring(0, idx);
    int cidx = host.indexOf(':'); if (cidx >= 0) host = host.substring(0, cidx);

    Serial.printf("[uploader] DNS precheck for %s -> host=%s RSSI=%d\n", uploadUrl.c_str(), host.c_str(), WiFi.RSSI());

    for (attempt = 1; attempt <= maxAttempts && !uploaded; attempt++) {
      // DNS check with extended retries
      IPAddress resolved;
      bool ok = false;
      for (int r=0; r<3; r++) {
        if (WiFi.hostByName(host.c_str(), resolved)) { ok = true; break; }
        Serial.printf("[uploader] DNS lookup attempt %d failed for %s, retrying...\n", r+1, host.c_str());
        delay(200 * (r+1));
      }
      if (!ok) {
        Serial.printf("[uploader] DNS lookup failed for %s (attempt %d)\n", host.c_str(), attempt);
        delay(backoffMs);
        backoffMs = min(backoffMs * 2, 15000);
        continue;
      }

      // Determine if TLS is required
      bool isTls = uploadUrl.startsWith("https://");
      // Quick TCP-level connect test to detect network / firewall issues before TLS/HTTP
      uint16_t port = isTls ? 443 : 80;
      WiFiClient tc;
      Serial.printf("[uploader] TCP connect test to %s:%u\n", resolved.toString().c_str(), port);
      if (!tc.connect(resolved, port)) {
        Serial.printf("[uploader] TCP connect to %s:%u failed (attempt %d) - network or remote host refusing connections\n", resolved.toString().c_str(), port, attempt);
        tc.stop();
        delay(backoffMs);
        backoffMs = min(backoffMs * 2, 15000);
        continue;
      } else {
        Serial.printf("[uploader] TCP connect to %s:%u succeeded\n", resolved.toString().c_str(), port);
        tc.stop();
      }

      WiFiClientSecure *secureClient = nullptr;
      bool began = false;

      if (isTls) {
        if (!persistentSecureInited) {
          // NOTE: setInsecure() is convenient for testing but not recommended for production
          Serial.println("[uploader] initializing persistent secure client (setInsecure)");
          persistentSecureClient.setInsecure();
          persistentSecureInited = true;
        }
        secureClient = &persistentSecureClient;
        // Explicitly pass host, port and path so SNI/Host are set correctly for TLS
        String pathOnly = "/";
        int hostPos = uploadUrl.indexOf(host);
        if (hostPos >= 0) {
          pathOnly = uploadUrl.substring(hostPos + host.length());
          if (pathOnly.length() == 0) pathOnly = "/";
        } else {
          int slashPos = uploadUrl.indexOf('/');
          if (slashPos >= 0) pathOnly = uploadUrl.substring(slashPos);
        }
        Serial.printf("[uploader] TLS begin host=%s path=%s port=%u\n", host.c_str(), pathOnly.c_str(), port);
        began = http.begin(*secureClient, uploadUrl.c_str());
      } else {
        began = http.begin(uploadUrl.c_str());
      }

      if (!began) {
        Serial.printf("[uploader] http.begin failed for %s (attempt %d)\n", uploadUrl.c_str(), attempt);
        delay(backoffMs);
        backoffMs = min(backoffMs * 2, 15000);
        continue;
      }

      http.addHeader("Content-Type", "application/octet-stream");
      if (apiKey.length() > 0) {
        http.addHeader("X-API-KEY", apiKey.c_str());
      }
      // Include optional headers to help Node register or resolve the device stream
      String streamUrl = uploader_get_stream_url();
      String deviceId = uploader_get_device_id();
      if (streamUrl.length() > 0) http.addHeader("X-STREAM-URL", streamUrl.c_str());
      if (deviceId.length() > 0) http.addHeader("X-DEVICE-ID", deviceId.c_str());

      // Ensure HTTPClient has a sensible timeout for network operations (seconds)
      http.setTimeout(60); // seconds
      unsigned long start = millis();
      int httpCode = http.sendRequest("POST", fb->buf, fb->len);
      Serial.printf("[uploader] sendRequest took %u ms, result=%d\n", (unsigned int)(millis() - start), httpCode);

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

            // Use TLS-aware registration too
            HTTPClient rh;
            if (regUrl.startsWith("https://")) {
              WiFiClientSecure *rclient = new WiFiClientSecure();
              rclient->setInsecure();
              if (rh.begin(*rclient, regUrl.c_str())) {
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
              } else {
                Serial.printf("[uploader] Stream register http.begin failed -> %s\n", regUrl.c_str());
              }
              rh.end();
              delete rclient;
            } else {
              if (rh.begin(regUrl.c_str())) {
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
              } else {
                Serial.printf("[uploader] Stream register http.begin failed -> %s\n", regUrl.c_str());
              }
              rh.end();
            }
          }
        }

        uploaded = true;

        // On success, attempt to drain queue if enabled
        if (uploader_is_queue_enabled()) {
          Serial.println("[uploader][queue] Upload succeeded, attempting to drain queue");
          // Drain loop: iterate files /uploadq/0..N-1 and upload sequentially
          int cap = uploader_get_queue_size();
          for (int i=0; i<cap; i++) {
            String path = String("/uploadq/") + String(i) + String(".bin");
            if (!LittleFS.exists(path)) continue;
            File f = LittleFS.open(path, "r");
            if (!f) { Serial.printf("[uploader][queue] failed to open %s\n", path.c_str()); continue; }
            size_t len = f.size();
            uint8_t *buf = (uint8_t*)malloc(len);
            if (!buf) { Serial.println("[uploader][queue] OOM reading queued frame"); f.close(); continue; }
            f.read(buf, len);
            f.close();

            // attempt to POST queued frame (single try)
            HTTPClient qhttp;
            bool qOk = false;
            if (uploadUrl.startsWith("https://")) {
              // Reuse persistent secure client created above to reduce TLS overhead
              if (qhttp.begin(persistentSecureClient, uploadUrl.c_str())) {
                qhttp.addHeader("Content-Type", "application/octet-stream");
                qhttp.setTimeout(60); // seconds
                unsigned long qstart = millis();
                int rc = qhttp.sendRequest("POST", buf, len);
                Serial.printf("[uploader][queue] sendRequest took %u ms, result=%d\n", (unsigned int)(millis() - qstart), rc);
                if (rc > 0) {
                  Serial.printf("[uploader][queue] POST %d -> %s\n", rc, uploadUrl.c_str());
                  qOk = true;
                } else {
                  Serial.printf("[uploader][queue] POST failed (%d) -> %s\n", rc, uploadUrl.c_str());
                }
              }
              qhttp.end();
            } else {
              if (qhttp.begin(uploadUrl.c_str())) {
                qhttp.addHeader("Content-Type", "application/octet-stream");
                qhttp.setTimeout(60); // seconds
                unsigned long qstart = millis();
                int rc = qhttp.sendRequest("POST", buf, len);
                Serial.printf("[uploader][queue] sendRequest took %u ms, result=%d\n", (unsigned int)(millis() - qstart), rc);
                if (rc > 0) {
                  Serial.printf("[uploader][queue] POST %d -> %s\n", rc, uploadUrl.c_str());
                  qOk = true;
                } else {
                  Serial.printf("[uploader][queue] POST failed (%d) -> %s\n", rc, uploadUrl.c_str());
                }
              }
              qhttp.end();
            }

            if (qOk) {
              // delete queued file
              LittleFS.remove(path);
              Serial.printf("[uploader][queue] drained and removed %s\n", path.c_str());
            } else {
              // stop if a queued upload failed to avoid burning cycles
              free(buf);
              break;
            }
            free(buf);
          }
        }

      } else {
        Serial.printf("[uploader] POST failed (%d) -> %s (attempt %d)\n", httpCode, uploadUrl.c_str(), attempt);
      }

      http.end();
      // Keep persistent secure client around to avoid repeated allocations and TLS re-handshakes

      if (!uploaded) {
        delay(backoffMs);
        backoffMs = min(backoffMs * 2, 30000);
      }
    }

    if (!uploaded) {
      Serial.printf("[uploader] giving up after %d attempts to %s\n", maxAttempts, uploadUrl.c_str());
      // Save frame to persistent queue if enabled
      if (uploader_is_queue_enabled()) {
        int cap = uploader_get_queue_size();
        // store in first available slot
        bool stored = false;
        for (int i=0; i<cap; i++) {
          String path = String("/uploadq/") + String(i) + String(".bin");
          if (!LittleFS.exists(path)) {
            File f = LittleFS.open(path, "w");
            if (!f) { Serial.printf("[uploader][queue] failed to open %s for write\n", path.c_str()); break; }
            f.write(fb->buf, fb->len);
            f.close();
            Serial.printf("[uploader][queue] saved frame to %s\n", path.c_str());
            stored = true;
            break;
          }
        }
        if (!stored) {
          // ring: overwrite oldest
          String path = String("/uploadq/0.bin");
          File f = LittleFS.open(path, "w");
          if (f) { f.write(fb->buf, fb->len); f.close(); Serial.printf("[uploader][queue] overwritten %s\n", path.c_str()); }
        }
      }
    }
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
  xTaskCreatePinnedToCore(uploaderTask, "uploader", 12 * 1024, NULL, 1, NULL, 1);
  uploader_started = true;
  Serial.println("[uploader] uploader task started");
#else
  Serial.println("[uploader] uploader is disabled (UPLOAD_ENABLED=false)");
#endif
}
