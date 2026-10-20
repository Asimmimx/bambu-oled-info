#pragma once
// Copy this file to secrets.h and fill it in. secrets.h is git-ignored.
// (Included only from src/Config.cpp, after PrinterConfig is declared.)

#define WIFI_SSID     "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// name       : shown on the top line (about 14 characters fit)
// shortName  : shown in the 6-printer overview grid (max 5 characters)
// ip         : printer LAN address - give each printer a DHCP reservation in your router
// serial     : printer settings -> device info (or Bambu Studio), NOT the "3DP-000-000" id
// accessCode : printer settings -> LAN / WLAN screen (8 characters)
static const PrinterConfig PRINTERS[] = {
    {"A1 mini 1", "MINI1", "192.168.1.101", "03000A000000001", "00000001"},
    {"A1 mini 2", "MINI2", "192.168.1.102", "03000A000000002", "00000002"},
    {"A1 mini 3", "MINI3", "192.168.1.103", "03000A000000003", "00000003"},
    {"A1",        "A1",    "192.168.1.104", "03900A000000004", "00000004"},
    {"X2D",       "X2D",   "192.168.1.105", "00000A000000005", "00000005"},
    {"A2L",       "A2L",   "192.168.1.106", "00000A000000006", "00000006"},
};
