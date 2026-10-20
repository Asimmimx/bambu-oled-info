#pragma once
// Copy this file to secrets.h and fill it in. secrets.h is git-ignored.
// (Included only from src/Config.cpp, after PrinterConfig is declared.)

#define WIFI_SSID     "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// Only needed for printers that are NOT on this Wi-Fi (e.g. in another house): they are read through
// Bambu Cloud. Do not type these by hand: `python tools/fetch_access_codes.py --token --cloud`
// writes them. The token is valid for about 3 months, then run the tool again.
//   #define BAMBU_CLOUD_USER  "u_0000000000"
//   #define BAMBU_CLOUD_TOKEN "eyJ..."

// name       : shown on the top line (about 14 characters fit)
// shortName  : shown in the overview grid (max 5 characters)
// ip         : printer LAN address; "" = reach this printer through Bambu Cloud
// serial     : printer settings -> device info (or Bambu Studio), NOT the "3DP-000-000" id
// accessCode : see tools/fetch_access_codes.py (ignored for cloud printers)
static const PrinterConfig PRINTERS[] = {
    {"A1 mini 1", "MINI1", "192.168.1.101", "03000A000000001", "00000001"},
    {"A1 mini 2", "MINI2", "192.168.1.102", "03000A000000002", "00000002"},
    {"A1 mini 3", "MINI3", "192.168.1.103", "03000A000000003", "00000003"},
    {"Other house", "OTHER", "", "03900A000000004", ""},  // reached through Bambu Cloud
};
