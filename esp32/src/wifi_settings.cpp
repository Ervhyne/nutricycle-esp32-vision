#include "wifi_settings.h"
#include <Preferences.h>

static Preferences prefs;
static const char *NS = "wifi";

// Simple percent-encoding (URL encode) helpers to avoid storing problematic raw control characters
static String pct_encode(const String &s) {
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

static String pct_decode(const String &s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '%' && i + 2 < s.length()) {
      char hi = s[i+1];
      char lo = s[i+2];
      int v = 0;
      auto hex = [](char ch)->int { if (ch >= '0' && ch <= '9') return ch - '0'; if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A'; if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a'; return 0; };
      v = (hex(hi) << 4) | hex(lo);
      out += (char)v;
      i += 2;
    } else if (c == '+') {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

void wifi_settings_init() {
  prefs.begin(NS, false);
}

String wifi_get_ssid() {
  String enc = prefs.getString("ssid", "");
  if (enc.length() == 0) return String("");
  String dec = pct_decode(enc);
  return dec;
}

String wifi_get_pass() {
  String enc = prefs.getString("pass", "");
  if (enc.length() == 0) return String("");
  String dec = pct_decode(enc);
  return dec;
}

bool wifi_is_provisioned() {
  String s = wifi_get_ssid();
  return s.length() > 0;
}

void wifi_set_credentials(const char *ssid, const char *pass) {
  // Log incoming values (lengths and quoted) to diagnose hidden/whitespace issues
  if (ssid) {
    Serial.printf("wifi_set_credentials called with ssid='%s' len=%d\n", ssid, (int)strlen(ssid));
  } else {
    Serial.println("wifi_set_credentials called with ssid=<null>");
  }
  if (pass) {
    Serial.printf("wifi_set_credentials called with pass_len=%d\n", (int)strlen(pass));
  } else {
    Serial.println("wifi_set_credentials called with pass=<null>");
  }

  if (ssid && strlen(ssid) > 0) {
    String enc = pct_encode(String(ssid));
    prefs.putString("ssid", enc);
    Serial.printf("wifi_set_credentials: stored encoded_ssid='%s' len=%d\n", enc.c_str(), (int)enc.length());
  }
  if (pass) {
    String encp = pct_encode(String(pass));
    prefs.putString("pass", encp);
    Serial.printf("wifi_set_credentials: stored encoded_pass_len=%d\n", (int)encp.length());
  }
}

void wifi_clear_credentials() {
  prefs.remove("ssid");
  prefs.remove("pass");
}
