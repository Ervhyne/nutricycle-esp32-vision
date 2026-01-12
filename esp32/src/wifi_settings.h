#ifndef WIFI_SETTINGS_H
#define WIFI_SETTINGS_H

#include <Arduino.h>

void wifi_settings_init();
String wifi_get_ssid();
String wifi_get_pass();
bool wifi_is_provisioned();
void wifi_set_credentials(const char *ssid, const char *pass);
void wifi_clear_credentials();

#endif // WIFI_SETTINGS_H
