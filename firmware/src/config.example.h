/*
 * Sentry AI - configuration template
 *
 * SETUP
 *   1. Copy this file to "config.h" in the same directory:
 *          cp config.example.h config.h
 *   2. Replace both placeholder values below.
 *   3. Flash the firmware.
 *
 * config.h is listed in .gitignore and must never be committed.
 * Do not reuse a password from any other network.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CONFIG_H
#define CONFIG_H

/*
 * Credentials for the Wi-Fi Access Point broadcast by the helmet itself
 * (the ESP32 runs in SoftAP mode - it does not join an existing network).
 *
 * WPA2 requires a password of at least 8 characters. Use a long, random
 * value; anyone who knows it can reach the helmet's HTTP control endpoints.
 */
const char* SSID     = "CHANGE_ME_SSID";
const char* PASSWORD = "CHANGE_ME_STRONG_PASSWORD";

#endif  // CONFIG_H
