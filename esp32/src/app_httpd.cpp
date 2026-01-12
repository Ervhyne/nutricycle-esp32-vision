// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include "camera_index.h"
#include "board_config.h"
#include "uploader_settings.h"
#include "uploader.h"
#include "wifi_settings.h"
#include "cJSON.h"
#include <WiFi.h>

#ifndef httpd_resp_send_400
#define httpd_resp_send_400(req) httpd_resp_send_err((req), 400, "Bad Request")
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#endif

// LED FLASH setup
#if defined(LED_GPIO_NUM)
#define CONFIG_LED_MAX_INTENSITY 255

int led_duty = 0;
bool isStreaming = false;

#endif

typedef struct {
  httpd_req_t *req;
  size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

typedef struct {
  size_t size;   //number of values used for filtering
  size_t index;  //current value index
  size_t count;  //value count
  int sum;
  int *values;  //array to be filled with values
} ra_filter_t;

static ra_filter_t ra_filter;

static ra_filter_t *ra_filter_init(ra_filter_t *filter, size_t sample_size) {
  memset(filter, 0, sizeof(ra_filter_t));

  filter->values = (int *)malloc(sample_size * sizeof(int));
  if (!filter->values) {
    return NULL;
  }
  memset(filter->values, 0, sample_size * sizeof(int));

  filter->size = sample_size;
  return filter;
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
static int ra_filter_run(ra_filter_t *filter, int value) {
  if (!filter->values) {
    return value;
  }
  filter->sum -= filter->values[filter->index];
  filter->values[filter->index] = value;
  filter->sum += filter->values[filter->index];
  filter->index++;
  filter->index = filter->index % filter->size;
  if (filter->count < filter->size) {
    filter->count++;
  }
  return filter->sum / filter->count;
}
#endif

#if defined(LED_GPIO_NUM)
void enable_led(bool en) {  // Turn LED On or Off
  int duty = en ? led_duty : 0;
  if (en && isStreaming && (led_duty > CONFIG_LED_MAX_INTENSITY)) {
    duty = CONFIG_LED_MAX_INTENSITY;
  }
  ledcWrite(LED_GPIO_NUM, duty);
  //ledc_set_duty(CONFIG_LED_LEDC_SPEED_MODE, CONFIG_LED_LEDC_CHANNEL, duty);
  //ledc_update_duty(CONFIG_LED_LEDC_SPEED_MODE, CONFIG_LED_LEDC_CHANNEL);
  log_i("Set LED intensity to %d", duty);
}
#endif

static esp_err_t bmp_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  uint64_t fr_start = esp_timer_get_time();
#endif
  fb = esp_camera_fb_get();
  if (!fb) {
    log_e("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/x-windows-bmp");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.bmp");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char ts[32];
  snprintf(ts, 32, "%lld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
  httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

  uint8_t *buf = NULL;
  size_t buf_len = 0;
  bool converted = frame2bmp(fb, &buf, &buf_len);
  esp_camera_fb_return(fb);
  if (!converted) {
    log_e("BMP Conversion failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  res = httpd_resp_send(req, (const char *)buf, buf_len);
  free(buf);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  uint64_t fr_end = esp_timer_get_time();
#endif
  log_i("BMP: %llums, %uB", (uint64_t)((fr_end - fr_start) / 1000), buf_len);
  return res;
}

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len) {
  jpg_chunking_t *j = (jpg_chunking_t *)arg;
  if (!index) {
    j->len = 0;
  }
  if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK) {
    return 0;
  }
  j->len += len;
  return len;
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  int64_t fr_start = esp_timer_get_time();
#endif

#if defined(LED_GPIO_NUM)
  enable_led(true);
  vTaskDelay(150 / portTICK_PERIOD_MS);  // The LED needs to be turned on ~150ms before the call to esp_camera_fb_get()
  fb = esp_camera_fb_get();              // or it won't be visible in the frame. A better way to do this is needed.
  enable_led(false);
#else
  fb = esp_camera_fb_get();
#endif

  if (!fb) {
    log_e("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char ts[32];
  snprintf(ts, 32, "%lld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
  httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  size_t fb_len = 0;
#endif
  if (fb->format == PIXFORMAT_JPEG) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    fb_len = fb->len;
#endif
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  } else {
    jpg_chunking_t jchunk = {req, 0};
    res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
    httpd_resp_send_chunk(req, NULL, 0);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    fb_len = jchunk.len;
#endif
  }
  esp_camera_fb_return(fb);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  int64_t fr_end = esp_timer_get_time();
#endif
  log_i("JPG: %uB %ums", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start) / 1000));
  return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  struct timeval _timestamp;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[128];

  log_i("Stream handler started - client connected");

  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");

#if defined(LED_GPIO_NUM)
  isStreaming = true;
  enable_led(true);
#endif

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      log_e("Camera capture failed - no frame buffer");
      res = ESP_FAIL;
    } else {
      _timestamp.tv_sec = fb->timestamp.tv_sec;
      _timestamp.tv_usec = fb->timestamp.tv_usec;
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          log_e("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      log_e("Send frame failed");
      break;
    }
    int64_t fr_end = esp_timer_get_time();

    int64_t frame_time = fr_end - last_frame;
    last_frame = fr_end;

    frame_time /= 1000;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
#endif
    log_i(
      "MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)", (uint32_t)(_jpg_buf_len), (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time, avg_frame_time,
      1000.0 / avg_frame_time
    );
  }

#if defined(LED_GPIO_NUM)
  isStreaming = false;
  enable_led(false);
#endif

  return res;
}

static esp_err_t parse_get(httpd_req_t *req, char **obuf) {
  char *buf = NULL;
  size_t buf_len = 0;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      *obuf = buf;
      return ESP_OK;
    }
    free(buf);
  }
  httpd_resp_send_404(req);
  return ESP_FAIL;
}

static esp_err_t cmd_handler(httpd_req_t *req) {
  char *buf = NULL;
  char variable[32];
  char value[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) != ESP_OK || httpd_query_key_value(buf, "val", value, sizeof(value)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int val = atoi(value);
  log_i("%s = %d", variable, val);
  sensor_t *s = esp_camera_sensor_get();
  int res = 0;

  if (!strcmp(variable, "framesize")) {
    if (s->pixformat == PIXFORMAT_JPEG) {
      res = s->set_framesize(s, (framesize_t)val);
    }
  } else if (!strcmp(variable, "quality")) {
    res = s->set_quality(s, val);
  } else if (!strcmp(variable, "contrast")) {
    res = s->set_contrast(s, val);
  } else if (!strcmp(variable, "brightness")) {
    res = s->set_brightness(s, val);
  } else if (!strcmp(variable, "saturation")) {
    res = s->set_saturation(s, val);
  } else if (!strcmp(variable, "gainceiling")) {
    res = s->set_gainceiling(s, (gainceiling_t)val);
  } else if (!strcmp(variable, "colorbar")) {
    res = s->set_colorbar(s, val);
  } else if (!strcmp(variable, "awb")) {
    res = s->set_whitebal(s, val);
  } else if (!strcmp(variable, "agc")) {
    res = s->set_gain_ctrl(s, val);
  } else if (!strcmp(variable, "aec")) {
    res = s->set_exposure_ctrl(s, val);
  } else if (!strcmp(variable, "hmirror")) {
    res = s->set_hmirror(s, val);
  } else if (!strcmp(variable, "vflip")) {
    res = s->set_vflip(s, val);
  } else if (!strcmp(variable, "awb_gain")) {
    res = s->set_awb_gain(s, val);
  } else if (!strcmp(variable, "agc_gain")) {
    res = s->set_agc_gain(s, val);
  } else if (!strcmp(variable, "aec_value")) {
    res = s->set_aec_value(s, val);
  } else if (!strcmp(variable, "aec2")) {
    res = s->set_aec2(s, val);
  } else if (!strcmp(variable, "dcw")) {
    res = s->set_dcw(s, val);
  } else if (!strcmp(variable, "bpc")) {
    res = s->set_bpc(s, val);
  } else if (!strcmp(variable, "wpc")) {
    res = s->set_wpc(s, val);
  } else if (!strcmp(variable, "raw_gma")) {
    res = s->set_raw_gma(s, val);
  } else if (!strcmp(variable, "lenc")) {
    res = s->set_lenc(s, val);
  } else if (!strcmp(variable, "special_effect")) {
    res = s->set_special_effect(s, val);
  } else if (!strcmp(variable, "wb_mode")) {
    res = s->set_wb_mode(s, val);
  } else if (!strcmp(variable, "ae_level")) {
    res = s->set_ae_level(s, val);
  }
#if defined(LED_GPIO_NUM)
  else if (!strcmp(variable, "led_intensity")) {
    led_duty = val;
    if (isStreaming) {
      enable_led(true);
    }
  }
#endif
  else {
    log_i("Unknown command: %s", variable);
    res = -1;
  }

  if (res < 0) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static int print_reg(char *p, sensor_t *s, uint16_t reg, uint32_t mask) {
  return sprintf(p, "\"0x%x\":%u,", reg, s->get_reg(s, reg, mask));
}

static esp_err_t status_handler(httpd_req_t *req) {
  static char json_response[1024];

  sensor_t *s = esp_camera_sensor_get();
  char *p = json_response;
  *p++ = '{';

  if (s->id.PID == OV5640_PID || s->id.PID == OV3660_PID) {
    for (int reg = 0x3400; reg < 0x3406; reg += 2) {
      p += print_reg(p, s, reg, 0xFFF);  //12 bit
    }
    p += print_reg(p, s, 0x3406, 0xFF);

    p += print_reg(p, s, 0x3500, 0xFFFF0);  //16 bit
    p += print_reg(p, s, 0x3503, 0xFF);
    p += print_reg(p, s, 0x350a, 0x3FF);   //10 bit
    p += print_reg(p, s, 0x350c, 0xFFFF);  //16 bit

    for (int reg = 0x5480; reg <= 0x5490; reg++) {
      p += print_reg(p, s, reg, 0xFF);
    }

    for (int reg = 0x5380; reg <= 0x538b; reg++) {
      p += print_reg(p, s, reg, 0xFF);
    }

    for (int reg = 0x5580; reg < 0x558a; reg++) {
      p += print_reg(p, s, reg, 0xFF);
    }
    p += print_reg(p, s, 0x558a, 0x1FF);  //9 bit
  } else if (s->id.PID == OV2640_PID) {
    p += print_reg(p, s, 0xd3, 0xFF);
    p += print_reg(p, s, 0x111, 0xFF);
    p += print_reg(p, s, 0x132, 0xFF);
  }

  p += sprintf(p, "\"xclk\":%u,", s->xclk_freq_hz / 1000000);
  p += sprintf(p, "\"pixformat\":%u,", s->pixformat);
  p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
  p += sprintf(p, "\"quality\":%u,", s->status.quality);
  p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
  p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
  p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
  p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
  p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
  p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
  p += sprintf(p, "\"awb\":%u,", s->status.awb);
  p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
  p += sprintf(p, "\"aec\":%u,", s->status.aec);
  p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
  p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
  p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
  p += sprintf(p, "\"agc\":%u,", s->status.agc);
  p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
  p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
  p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
  p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
  p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
  p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
  p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
  p += sprintf(p, "\"vflip\":%u,", s->status.vflip);
  p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
  p += sprintf(p, "\"colorbar\":%u", s->status.colorbar);
#if defined(LED_GPIO_NUM)
  p += sprintf(p, ",\"led_intensity\":%u", led_duty);
#else
  p += sprintf(p, ",\"led_intensity\":%d", -1);
#endif
  *p++ = '}';
  *p++ = 0;
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t xclk_handler(httpd_req_t *req) {
  char *buf = NULL;
  char _xclk[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "xclk", _xclk, sizeof(_xclk)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int xclk = atoi(_xclk);
  log_i("Set XCLK: %d MHz", xclk);

  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_xclk(s, LEDC_TIMER_0, xclk);
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t reg_handler(httpd_req_t *req) {
  char *buf = NULL;
  char _reg[32];
  char _mask[32];
  char _val[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "reg", _reg, sizeof(_reg)) != ESP_OK || httpd_query_key_value(buf, "mask", _mask, sizeof(_mask)) != ESP_OK
      || httpd_query_key_value(buf, "val", _val, sizeof(_val)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int reg = atoi(_reg);
  int mask = atoi(_mask);
  int val = atoi(_val);
  log_i("Set Register: reg: 0x%02x, mask: 0x%02x, value: 0x%02x", reg, mask, val);

  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_reg(s, reg, mask, val);
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t greg_handler(httpd_req_t *req) {
  char *buf = NULL;
  char _reg[32];
  char _mask[32];

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  if (httpd_query_key_value(buf, "reg", _reg, sizeof(_reg)) != ESP_OK || httpd_query_key_value(buf, "mask", _mask, sizeof(_mask)) != ESP_OK) {
    free(buf);
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  free(buf);

  int reg = atoi(_reg);
  int mask = atoi(_mask);
  sensor_t *s = esp_camera_sensor_get();
  int res = s->get_reg(s, reg, mask);
  if (res < 0) {
    return httpd_resp_send_500(req);
  }
  log_i("Get Register: reg: 0x%02x, mask: 0x%02x, value: 0x%02x", reg, mask, res);

  char buffer[20];
  const char *val = itoa(res, buffer, 10);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, val, strlen(val));
}

static int parse_get_var(char *buf, const char *key, int def) {
  char _int[16];
  if (httpd_query_key_value(buf, key, _int, sizeof(_int)) != ESP_OK) {
    return def;
  }
  return atoi(_int);
}

static esp_err_t pll_handler(httpd_req_t *req) {
  char *buf = NULL;

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }

  int bypass = parse_get_var(buf, "bypass", 0);
  int mul = parse_get_var(buf, "mul", 0);
  int sys = parse_get_var(buf, "sys", 0);
  int root = parse_get_var(buf, "root", 0);
  int pre = parse_get_var(buf, "pre", 0);
  int seld5 = parse_get_var(buf, "seld5", 0);
  int pclken = parse_get_var(buf, "pclken", 0);
  int pclk = parse_get_var(buf, "pclk", 0);
  free(buf);

  log_i("Set Pll: bypass: %d, mul: %d, sys: %d, root: %d, pre: %d, seld5: %d, pclken: %d, pclk: %d", bypass, mul, sys, root, pre, seld5, pclken, pclk);
  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_pll(s, bypass, mul, sys, root, pre, seld5, pclken, pclk);
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t win_handler(httpd_req_t *req) {
  char *buf = NULL;

  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }

  int startX = parse_get_var(buf, "sx", 0);
  int startY = parse_get_var(buf, "sy", 0);
  int endX = parse_get_var(buf, "ex", 0);
  int endY = parse_get_var(buf, "ey", 0);
  int offsetX = parse_get_var(buf, "offx", 0);
  int offsetY = parse_get_var(buf, "offy", 0);
  int totalX = parse_get_var(buf, "tx", 0);
  int totalY = parse_get_var(buf, "ty", 0);  // codespell:ignore totaly
  int outputX = parse_get_var(buf, "ox", 0);
  int outputY = parse_get_var(buf, "oy", 0);
  bool scale = parse_get_var(buf, "scale", 0) == 1;
  bool binning = parse_get_var(buf, "binning", 0) == 1;
  free(buf);

  log_i(
    "Set Window: Start: %d %d, End: %d %d, Offset: %d %d, Total: %d %d, Output: %d %d, Scale: %u, Binning: %u", startX, startY, endX, endY, offsetX, offsetY,
    totalX, totalY, outputX, outputY, scale, binning  // codespell:ignore totaly
  );
  sensor_t *s = esp_camera_sensor_get();
  int res = s->set_res_raw(s, startX, startY, endX, endY, offsetX, offsetY, totalX, totalY, outputX, outputY, scale, binning);  // codespell:ignore totaly
  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t uploader_get_handler(httpd_req_t *req) {
  log_i("HTTP: /uploader GET requested");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  uploader_settings_init();
  String url = uploader_get_url();
  String api = uploader_get_api_key();
  uint32_t interval = uploader_get_interval_ms();

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "url", url.c_str());
  cJSON_AddStringToObject(root, "gateway", uploader_get_gateway().c_str());
  cJSON_AddStringToObject(root, "api_key", api.c_str());
  cJSON_AddNumberToObject(root, "interval_ms", interval);
  // include device id and (optional) public stream URL
  cJSON_AddStringToObject(root, "device_id", uploader_get_device_id().c_str());
  cJSON_AddStringToObject(root, "stream_url", uploader_get_stream_url().c_str());

  char *out = cJSON_PrintUnformatted(root);
  httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
  cJSON_free(out);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t uploader_post_handler(httpd_req_t *req) {
  log_i("HTTP: /uploader POST requested (len=%d)", req->content_len);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  int len = req->content_len;
  Serial.printf("[uploader] handler entered; content_len=%d\n", len);
  if (len <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  char *buf = (char*)malloc(len + 1);
  if (!buf) {
    Serial.println("[uploader] malloc failed");
    return httpd_resp_send_500(req);
  }
  int ret = httpd_req_recv(req, buf, len);
  if (ret <= 0) {
    Serial.printf("[uploader] httpd_req_recv failed ret=%d\n", ret);
    free(buf);
    return httpd_resp_send_500(req);
  }
  buf[len] = 0;
  Serial.printf("[uploader] body=%s\n", buf);

  cJSON *root = cJSON_Parse(buf);
  free(buf);
  if (!root) {
    Serial.println("[uploader] JSON parse failed");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Bad Request - invalid JSON", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  cJSON *jurl = cJSON_GetObjectItem(root, "url");
  cJSON *jgateway = cJSON_GetObjectItem(root, "gateway");
  cJSON *japi = cJSON_GetObjectItem(root, "api_key");
  cJSON *jinterval = cJSON_GetObjectItem(root, "interval_ms");
  cJSON *jdevice = cJSON_GetObjectItem(root, "device_id");
  cJSON *jstream = cJSON_GetObjectItem(root, "stream_url");

  if (jurl && cJSON_IsString(jurl)) {
    uploader_set_url(jurl->valuestring);
    Serial.printf("HTTP /uploader: saved url='%s'\n", jurl->valuestring);
  }
  if (jgateway && cJSON_IsString(jgateway)) {
    uploader_set_gateway(jgateway->valuestring);
    Serial.printf("HTTP /uploader: saved gateway='%s'\n", jgateway->valuestring);
  }
  if (japi && cJSON_IsString(japi)) {
    uploader_set_api_key(japi->valuestring);
    Serial.printf("HTTP /uploader: saved api_key_len=%d\n", (int)strlen(japi->valuestring));
  }
  if (jinterval && cJSON_IsNumber(jinterval)) {
    uploader_set_interval_ms((uint32_t)jinterval->valuedouble);
    Serial.printf("HTTP /uploader: saved interval_ms=%u\n", (uint32_t)jinterval->valuedouble);
  }
  if (jdevice && cJSON_IsString(jdevice)) {
    uploader_set_device_id(jdevice->valuestring);
    Serial.printf("HTTP /uploader: saved device_id='%s'\n", jdevice->valuestring);
  }
  if (jstream && cJSON_IsString(jstream)) {
    uploader_set_stream_url(jstream->valuestring);
    Serial.printf("HTTP /uploader: saved stream_url='%s'\n", jstream->valuestring);
  }

  cJSON_Delete(root);

  cJSON *out = cJSON_CreateObject();
  cJSON_AddBoolToObject(out, "ok", true);
  cJSON_AddStringToObject(out, "gateway", uploader_get_gateway().c_str());
  cJSON_AddStringToObject(out, "device_id", uploader_get_device_id().c_str());
  char *sout = cJSON_PrintUnformatted(out);
  httpd_resp_send(req, sout, HTTPD_RESP_USE_STRLEN);
  cJSON_free(sout);
  cJSON_Delete(out);
  return ESP_OK;
}

static esp_err_t wifi_get_handler(httpd_req_t *req) {
  log_i("HTTP: /wifi GET requested");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  wifi_settings_init();
  String ssid = wifi_get_ssid();
  bool prov = wifi_is_provisioned();

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "ssid", ssid.c_str());
  cJSON_AddBoolToObject(root, "provisioned", prov);

  char *out = cJSON_PrintUnformatted(root);
  httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
  cJSON_free(out);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t wifi_post_handler(httpd_req_t *req) {
  log_i("HTTP: /wifi POST requested (len=%d)", req->content_len);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  int len = req->content_len;
  if (len <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  char *buf = (char*)malloc(len + 1);
  if (!buf) return httpd_resp_send_500(req);
  int ret = httpd_req_recv(req, buf, len);
  if (ret <= 0) {
    free(buf);
    return httpd_resp_send_500(req);
  }
  buf[len] = 0;

  // Debug: log raw request body to help diagnose provisioning issues
  Serial.printf("HTTP: /wifi POST body: %s\n", buf);

  cJSON *root = cJSON_Parse(buf);
  free(buf);
  if (!root) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  cJSON *jssid = cJSON_GetObjectItem(root, "ssid");
  cJSON *jpass = cJSON_GetObjectItem(root, "password");

  // Accept SSID even when password is empty/missing (open Wi‑Fi networks or when user leaves password blank).
  // Save only SSID & password if provided
  if (jssid && cJSON_IsString(jssid)) {
    const char *ssidstr = jssid->valuestring;
    const char *passstr = (jpass && cJSON_IsString(jpass)) ? jpass->valuestring : "";
    wifi_set_credentials(ssidstr, passstr);
    Serial.printf("HTTP /wifi: provisioning saved (decoded) ssid='%s' pass_len=%d\n", ssidstr, (int)strlen(passstr));
  }

  cJSON_Delete(root);

  const char *ok = "{\"ok\":true}";
  httpd_resp_send(req, ok, strlen(ok));

  // Attempt to reconnect in background (non-blocking short task)
  xTaskCreatePinnedToCore([](void *param) {
    (void)param;
    String ssid = wifi_get_ssid();
    String pass = wifi_get_pass();
    if (ssid.length() == 0) vTaskDelete(NULL);

    Serial.printf("[provision] Attempting reconnection to SSID: '%s' (len=%d) pass_len=%d\n", ssid.c_str(), (int)ssid.length(), (int)pass.length());

    int n = WiFi.scanNetworks();
    Serial.printf("[provision] Scan found %d networks\n", n);
    for (int i = 0; i < n; i++) {
      Serial.printf("  %d: '%s' (%d dBm)\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }

    // Keep AP active during connect attempts
    WiFi.disconnect(false); // don't erase stored AP config
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (millis() - start < 10000) {
      if (WiFi.status() == WL_CONNECTED) break;
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[provision] Reconnected: %s\n", WiFi.localIP().toString().c_str());
      // re-enable SoftAP so provisioning UI remains available
      const char* apName = "NutriCycle-Setup";
      WiFi.softAP(apName);
      Serial.printf("[provision] SoftAP '%s' ensured for provisioning\n", apName);
    } else {
      Serial.println("[provision] Reconnect attempt failed");
    }

    vTaskDelete(NULL);
  }, "reconnect", 4 * 1024, NULL, 1, NULL, 1);

  return ESP_OK;
}

static esp_err_t reconnect_handler(httpd_req_t *req) {
  log_i("HTTP: /reconnect POST requested");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  wifi_settings_init();
  String ssid = wifi_get_ssid();
  String pass = wifi_get_pass();

  if (ssid.length() == 0) {
    const char *err = "{\"ok\":false, \"error\": \"no_credentials\"}";
    httpd_resp_send(req, err, strlen(err));
    return ESP_OK;
  }

  // Attempt to connect (blocking up to 8s) to report immediate status
  Serial.printf("[reconnect] Attempting connect to SSID: '%s' (len=%d) pass_len=%d\n", ssid.c_str(), (int)ssid.length(), (int)pass.length());

  int n = WiFi.scanNetworks();
  Serial.printf("[reconnect] Scan found %d networks\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  %d: '%s' (%d dBm)\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }

  WiFi.disconnect(false); // do not erase credentials
  // Keep AP active while attempting to connect so provisioning UI stays available
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (millis() - start < 8000) {
    if (WiFi.status() == WL_CONNECTED) break;
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  char out[256];
  if (WiFi.status() == WL_CONNECTED) {
    // Ensure SoftAP is still available for provisioning use
    const char* apName = "NutriCycle-Setup";
    WiFi.softAP(apName);
    Serial.printf("[reconnect] Reconnected: %s; SoftAP '%s' ensured\n", WiFi.localIP().toString().c_str(), apName);
    snprintf(out, sizeof(out), "{\"ok\":true, \"connected\":true, \"ip\":\"%s\"}", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[reconnect] Reconnect attempt failed");
    snprintf(out, sizeof(out), "{\"ok\":true, \"connected\":false}");
  }

  httpd_resp_send(req, out, strlen(out));
  return ESP_OK;
}

static esp_err_t provision_post_handler(httpd_req_t *req) {
  log_i("HTTP: /provision POST requested");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  int len = req->content_len;
  if (len <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  char *buf = (char*)malloc(len + 1);
  if (!buf) return httpd_resp_send_500(req);
  int ret = httpd_req_recv(req, buf, len);
  if (ret <= 0) { free(buf); return httpd_resp_send_500(req); }
  buf[len] = 0;

  Serial.printf("HTTP: /provision POST body: %s\n", buf);
  cJSON *root = cJSON_Parse(buf);
  free(buf);
  if (!root) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  cJSON *jssid = cJSON_GetObjectItem(root, "ssid");
  cJSON *jpass = cJSON_GetObjectItem(root, "password");
  cJSON *jurl = cJSON_GetObjectItem(root, "url");
  cJSON *jgateway = cJSON_GetObjectItem(root, "gateway");
  cJSON *japi = cJSON_GetObjectItem(root, "api_key");
  cJSON *jinterval = cJSON_GetObjectItem(root, "interval_ms");
  cJSON *jdevice = cJSON_GetObjectItem(root, "device_id");
  cJSON *jstream = cJSON_GetObjectItem(root, "stream_url");

  if (jssid && cJSON_IsString(jssid)) {
    const char *ssidstr = jssid->valuestring;
    const char *passstr = (jpass && cJSON_IsString(jpass)) ? jpass->valuestring : "";
    wifi_set_credentials(ssidstr, passstr);
    Serial.printf("/provision saved wifi (decoded) ssid='%s' pass_len=%d\n", ssidstr, (int)strlen(passstr));
  }

  if (jurl && cJSON_IsString(jurl)) {
    uploader_set_url(jurl->valuestring);
    Serial.printf("/provision saved uploader url='%s'\n", jurl->valuestring);
  }
  if (jgateway && cJSON_IsString(jgateway)) {
    uploader_set_gateway(jgateway->valuestring);
    Serial.printf("/provision saved uploader gateway='%s'\n", jgateway->valuestring);
  }
  if (japi && cJSON_IsString(japi)) {
    uploader_set_api_key(japi->valuestring);
    Serial.printf("/provision saved uploader api_key_len=%d\n", (int)strlen(japi->valuestring));
  }
  if (jinterval && cJSON_IsNumber(jinterval)) {
    uploader_set_interval_ms((uint32_t)jinterval->valuedouble);
    Serial.printf("/provision saved uploader interval_ms=%u\n", (uint32_t)jinterval->valuedouble);
  }
  if (jdevice && cJSON_IsString(jdevice)) {
    uploader_set_device_id(jdevice->valuestring);
    Serial.printf("/provision saved uploader device_id='%s'\n", jdevice->valuestring);
  }
  if (jstream && cJSON_IsString(jstream)) {
    uploader_set_stream_url(jstream->valuestring);
    Serial.printf("/provision saved uploader stream_url='%s'\n", jstream->valuestring);
  }

  cJSON_Delete(root);

  const char *ok = "{\"ok\":true}";
  httpd_resp_send(req, ok, strlen(ok));

  // Background reconnect + ensure AP stays on
  xTaskCreatePinnedToCore([](void *param) {
    (void)param;
    String ssid = wifi_get_ssid();
    String pass = wifi_get_pass();
    if (ssid.length() == 0) vTaskDelete(NULL);

    Serial.printf("[provision] Starting reconnection attempt to '%s'\n", ssid.c_str());

    int n = WiFi.scanNetworks();
    Serial.printf("[provision] Scan found %d networks\n", n);
    for (int i = 0; i < n; ++i) {
      Serial.printf("[provision] scan[%d] SSID='%s' RSSI=%d secure=%d\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i));
    }

    WiFi.disconnect(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    unsigned long last_log = 0;
    while (millis() - start < 15000) {
      if (WiFi.status() == WL_CONNECTED) break;
      if (millis() - last_log >= 2000) {
        Serial.printf("[provision] waiting for connect... status=%d elapsed=%lums\n", WiFi.status(), millis() - start);
        last_log = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[provision] Reconnected: %s\n", WiFi.localIP().toString().c_str());
      const char* apName = "NutriCycle-Setup";
      WiFi.softAP(apName);
      Serial.printf("[provision] SoftAP '%s' ensured for provisioning\n", apName);

      // If uploader configured and wifi connected, start uploader task
      uploader_settings_init();
      if (uploader_is_configured()) {
        Serial.println("[provision] Uploader configured; attempting to start uploader task");
        startUploaderTask();
      }
    } else {
      Serial.println("[provision] Reconnect attempt failed");
    }

    vTaskDelete(NULL);
  }, "provision_reconnect", 6 * 1024, NULL, 1, NULL, 1);

  Serial.println("[provision] Background reconnect task started");
  return ESP_OK;
}

static esp_err_t start_ap_handler(httpd_req_t *req) {
  log_i("HTTP: /start_ap POST requested");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  WiFi.mode(WIFI_AP_STA);
  const char* apName = "NutriCycle-Setup";
  WiFi.softAP(apName);
  log_i("SoftAP '%s' started for provisioning", apName);
  Serial.printf("HTTP /start_ap: SoftAP '%s' started\n", apName);

  const char *ok = "{\"ok\":true}";
  return httpd_resp_send(req, ok, strlen(ok));
}

static const char *setup_html = R"rawliteral(<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>NutriCycle Setup</title></head><body style=\"font-family:Arial,sans-serif;padding:12px\"> 
<h2>NutriCycle - Device Setup</h2>
<p>Use this page to configure Wi‑Fi and Node gateway settings. Enter the gateway host or full URL (e.g. <code>example.ngrok.io</code> or <code>https://example.ngrok.io</code>). The API key is optional and used if your gateway requires it. Leave Gateway blank to rely on direct MJPEG streaming and device mapping on the gateway. Recommended: use the uploader mode (enter gateway) so the device can POST frames to the gateway which forwards detection requests.</p>
<form id="provisionForm">
  <h3>Wi-Fi</h3>
  <label>SSID: <input id="ssid" name="ssid" /></label><br/><br/>
  <label>Password: <input id="password" name="password" type="password" /></label><br/><br/>

  <h3>Uploader</h3>
  <label>Gateway host or URL: <input id="gateway" name="gateway" style="width:80%" placeholder="example.ngrok.io or https://example.ngrok.io" /></label>
  <small>Enter the Node gateway host (e.g. <code>192.168.1.50:3000</code>) or full URL (e.g. <code>https://example.ngrok.io</code>). The device will post frames to <code>&lt;gateway&gt;/upload</code>. Use the API key below if your gateway requires it (header <code>X-API-KEY</code>).</small><br/><br/>
  <label>API Key: <input id="api_key" name="api_key" /></label><br/><br/>
  <label>Device ID: <input id="device_id" name="device_id" /></label>
  <small>(defaults to device MAC address; used to register the stream on the gateway)</small><br/><br/>
  <label>Stream URL (optional): <input id="stream_url" name="stream_url" style="width:80%" placeholder="https://your-ngrok-url.ngrok.io/stream" /></label>
  <small>Optional public MJPEG stream URL (ngrok or router). If provided, the device will attempt to register this URL with the Node gateway on save.</small><br/><br/>
  <label>Interval (ms): <input id="interval_ms" name="interval_ms" type="number" /></label>
  <small>Uploader frame POST interval (default 500ms). If you want live MJPEG via Node proxy, leave Gateway blank and ensure Node has device mapping.</small><br/><br/>

  <button type="button" onclick="saveUploaderOnly()">Save Uploader Only</button>
  <button type="button" onclick="saveProvision()">Save & Apply</button>
</form>
<hr/>
<div id="msg"></div>
<script>
async function load() {
  try {
    const r1 = await fetch('/wifi');
    const w = await r1.json();
    document.getElementById('ssid').value = w.ssid || '';

    const r2 = await fetch('/uploader');
    const u = await r2.json();
    document.getElementById('gateway').value = u.gateway || u.url || '';
    document.getElementById('api_key').value = u.api_key||'';
    document.getElementById('device_id').value = u.device_id||'';
    document.getElementById('stream_url').value = u.stream_url||'';
    document.getElementById('interval_ms').value = u.interval_ms||500;

    // NOTE: single-button provision function is available: saveProvision()
  } catch (e) { console.error(e); }
}
async function saveWifi() {
  const ssid = document.getElementById('ssid').value;
  const password = document.getElementById('password').value;
  await fetch('/wifi', { method: 'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid,password}) });
  document.getElementById('msg').innerText = 'Wi-Fi saved. Device will try to connect.';
}
async function saveUploader() {
  const gateway = document.getElementById('gateway').value;
  const api_key = document.getElementById('api_key').value;
  const device_id = document.getElementById('device_id').value;
  const stream_url = document.getElementById('stream_url').value;
  const interval_ms = Number(document.getElementById('interval_ms').value) || 500;
  try {
    const r = await fetch('/uploader', { method: 'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({gateway,api_key,device_id,stream_url,interval_ms}) });
    const text = await r.text();
    if (!r.ok) {
      document.getElementById('msg').innerText = `Uploader save failed: ${r.status} ${text}`;
      return {ok:false, status:r.status, text};
    }
    document.getElementById('msg').innerText = 'Uploader settings saved: ' + text;
    return {ok:true, status:r.status, text};
  } catch (e) {
    document.getElementById('msg').innerText = 'Uploader save error: ' + e.toString();
    return {ok:false, status:0, text:e.toString()};
  }
}
async function saveUploaderOnly() {
  document.getElementById('msg').innerText = 'Saving uploader (no wifi change)...';
  await saveUploader();
}
async function reconnect() {
  document.getElementById('msg').innerText = 'Attempting reconnect...';
  try {
    const r = await fetch('/reconnect', { method: 'POST' });
    const j = await r.json();
    if (j && j.connected) {
      document.getElementById('msg').innerText = 'Reconnected! IP: ' + (j.ip || '');
    } else {
      document.getElementById('msg').innerText = 'Reconnect failed. Status: ' + (j && j.connected ? 'connected' : 'not connected');
    }
  } catch (e) {
    document.getElementById('msg').innerText = 'Reconnect error';
  }
}
// Single-button provision function (JS)
async function saveProvision() {
  const ssid = document.getElementById('ssid').value;
  const password = document.getElementById('password').value;
  const gateway = document.getElementById('gateway').value;
  const api_key = document.getElementById('api_key').value;
  const device_id = document.getElementById('device_id').value;
  const stream_url = document.getElementById('stream_url').value;
  const interval_ms = Number(document.getElementById('interval_ms').value) || 500;

  document.getElementById('msg').innerText = 'Saving uploader settings...';
  try {
    // First save uploader settings (so they persist even if reconnect interrupts the AP)
    const up = await saveUploader();
    if (!up.ok) {
      document.getElementById('msg').innerText = `Uploader save failed: ${up.status} ${up.text}`;
      return;
    }

    document.getElementById('msg').innerText = 'Uploader saved. Saving Wi‑Fi and applying...';

    // Then save wifi (this will trigger reconnect) — send minimal body to /provision
    try {
      const provResp = await fetch('/provision', { method: 'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ssid,password}) });
      let provText = '';
      try { provText = await provResp.text(); } catch(e) {}
      if (!provResp.ok) {
        document.getElementById('msg').innerText = `Provision save failed: ${provResp.status} ${provText}`;
        return;
      }

      // provision responded ok; now check reconnect status
      document.getElementById('msg').innerText = 'Saved. Attempting reconnect...';
      try {
        const r2 = await fetch('/reconnect', { method: 'POST' });
        const j2 = await r2.json();
        if (j2 && j2.connected) {
          document.getElementById('msg').innerText = 'Reconnected! IP: ' + (j2.ip || '');
        } else {
          document.getElementById('msg').innerText = 'Saved. Reconnect failed or pending; SoftAP remains active for provisioning.';
        }
      } catch (e) { document.getElementById('msg').innerText = 'Saved, but reconnect probe failed'; }

      // Attempt stream registration (best-effort) using saved gateway
      if (gateway && stream_url && device_id) {
        try {
          let base = gateway;
          if (!/^https?:\/\//.test(base)) base = 'http://' + base;
          base = (new URL(base)).origin;
          const regUrl = `${base}/devices/${encodeURIComponent(device_id)}/register_stream`;
          const rreg = await fetch(regUrl, { method: 'POST', headers: {'Content-Type':'application/json', 'X-API-KEY': api_key || ''}, body: JSON.stringify({ url: stream_url }) });
          if (rreg.ok) {
            document.getElementById('msg').innerText += ' Stream registered with gateway.';
          } else {
            document.getElementById('msg').innerText += ' Stream registration failed.';
          }
        } catch (e) {
          document.getElementById('msg').innerText += ' Stream registration error.';
        }
      }

    } catch (e) {
      document.getElementById('msg').innerText = 'Error saving provisioning: ' + e.toString();
      return;
    }

  } catch (e) {
    document.getElementById('msg').innerText = 'Error saving uploader settings: ' + e.toString();
  }
}

async function startAP() {
    document.getElementById('msg').innerText = 'Starting AP...';
    try {
      const r = await fetch('/start_ap', { method: 'POST' });
      const j = await r.json();
      if (j && j.ok) document.getElementById('msg').innerText = 'AP started. Connect to 192.168.4.1';
      else document.getElementById('msg').innerText = 'Failed to start AP';
    } catch (e) { document.getElementById('msg').innerText = 'Error starting AP'; }
  }
  function injectOpenAPButton() {
    if (!document.getElementById('openApBtn')) {
      let b = document.createElement('button');
      b.type = 'button';
      b.id = 'openApBtn';
      b.innerText = 'Open Setup AP';
      b.style.marginLeft = '8px';
      b.onclick = startAP;
      let wf = document.getElementById('wifiForm'); if (wf) wf.appendChild(b);
    }
  }
  window.addEventListener('load', () => { load(); injectOpenAPButton(); });
</script>
</body>
</html>)rawliteral";

static esp_err_t setup_handler(httpd_req_t *req) {
  log_i("HTTP: /setup requested");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, setup_html, strlen(setup_html));
}

static esp_err_t index_handler(httpd_req_t *req) {
  // If device is not provisioned, redirect to the setup page to make provisioning UX easier
  wifi_settings_init();
  if (!wifi_is_provisioned()) {
    log_i("Index requested while not provisioned - serving setup page");
    return setup_handler(req);
  }

  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    if (s->id.PID == OV3660_PID) {
      return httpd_resp_send(req, (const char *)index_ov3660_html_gz, index_ov3660_html_gz_len);
    } else if (s->id.PID == OV5640_PID) {
      return httpd_resp_send(req, (const char *)index_ov5640_html_gz, index_ov5640_html_gz_len);
    } else {
      return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
    }
  } else {
    log_e("Camera sensor not found");
    return httpd_resp_send_500(req);
  }
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 32; // allow extra handlers for settings

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t cmd_uri = {
    .uri = "/control",
    .method = HTTP_GET,
    .handler = cmd_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t capture_uri = {
    .uri = "/capture",
    .method = HTTP_GET,
    .handler = capture_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t bmp_uri = {
    .uri = "/bmp",
    .method = HTTP_GET,
    .handler = bmp_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t xclk_uri = {
    .uri = "/xclk",
    .method = HTTP_GET,
    .handler = xclk_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t reg_uri = {
    .uri = "/reg",
    .method = HTTP_GET,
    .handler = reg_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t greg_uri = {
    .uri = "/greg",
    .method = HTTP_GET,
    .handler = greg_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  /*
   * Simple JSON API endpoints to view & update uploader settings (upload URL, API key, interval)
   * GET  /uploader -> returns { url, api_key, interval_ms }
   * POST /uploader -> accepts JSON { url, api_key, interval_ms }
   */

  httpd_uri_t uploader_get_uri = {
    .uri = "/uploader",
    .method = HTTP_GET,
    .handler = uploader_get_handler,
    .user_ctx = NULL
  };

  httpd_uri_t uploader_post_uri = {
    .uri = "/uploader",
    .method = HTTP_POST,
    .handler = uploader_post_handler,
    .user_ctx = NULL
  };

  // Wi-Fi endpoints (GET returns stored SSID/provision state, POST saves credentials)
  httpd_uri_t wifi_get_uri = {
    .uri = "/wifi",
    .method = HTTP_GET,
    .handler = wifi_get_handler,
    .user_ctx = NULL
  };

  httpd_uri_t wifi_post_uri = {
    .uri = "/wifi",
    .method = HTTP_POST,
    .handler = wifi_post_handler,
    .user_ctx = NULL
  };

  // Register uploader and wifi endpoints
  httpd_register_uri_handler(camera_httpd, &uploader_get_uri);
  httpd_register_uri_handler(camera_httpd, &uploader_post_uri);
  httpd_register_uri_handler(camera_httpd, &wifi_get_uri);
  httpd_register_uri_handler(camera_httpd, &wifi_post_uri);

  // Combined provisioning endpoint (single button config)
  httpd_uri_t provision_post_uri = {
    .uri = "/provision",
    .method = HTTP_POST,
    .handler = provision_post_handler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(camera_httpd, &provision_post_uri);

  // Setup page for provisioning and uploader settings
  httpd_uri_t setup_uri = {
    .uri = "/setup",
    .method = HTTP_GET,
    .handler = setup_handler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(camera_httpd, &setup_uri);

  // Reconnect endpoint to attempt Wi-Fi connection immediately
  httpd_uri_t reconnect_uri = {
    .uri = "/reconnect",
    .method = HTTP_POST,
    .handler = reconnect_handler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(camera_httpd, &reconnect_uri);

  // Endpoint to reopen SoftAP for provisioning while STA is active
  httpd_uri_t startap_uri = {
    .uri = "/start_ap",
    .method = HTTP_POST,
    .handler = start_ap_handler,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(camera_httpd, &startap_uri);

  httpd_uri_t pll_uri = {
    .uri = "/pll",
    .method = HTTP_GET,
    .handler = pll_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t win_uri = {
    .uri = "/resolution",
    .method = HTTP_GET,
    .handler = win_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  ra_filter_init(&ra_filter, 20);

  log_i("Starting web server on port: '%d'", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &bmp_uri);

    httpd_register_uri_handler(camera_httpd, &xclk_uri);
    httpd_register_uri_handler(camera_httpd, &reg_uri);
    httpd_register_uri_handler(camera_httpd, &greg_uri);
    httpd_register_uri_handler(camera_httpd, &pll_uri);
    httpd_register_uri_handler(camera_httpd, &win_uri);

    // Ensure uploader, wifi & provisioning endpoints are registered after server start
    httpd_register_uri_handler(camera_httpd, &uploader_get_uri);
    httpd_register_uri_handler(camera_httpd, &uploader_post_uri);
    httpd_register_uri_handler(camera_httpd, &wifi_get_uri);
    httpd_register_uri_handler(camera_httpd, &wifi_post_uri);
    httpd_register_uri_handler(camera_httpd, &provision_post_uri);
    httpd_register_uri_handler(camera_httpd, &setup_uri);
    httpd_register_uri_handler(camera_httpd, &reconnect_uri);
    httpd_register_uri_handler(camera_httpd, &startap_uri);
  }

  config.server_port += 1;
  config.ctrl_port += 1;
  log_i("Starting stream server on port: '%d'", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setupLedFlash() {
#if defined(LED_GPIO_NUM)
  ledcAttach(LED_GPIO_NUM, 5000, 8);
#else
  log_i("LED flash is disabled -> LED_GPIO_NUM undefined");
#endif
}
