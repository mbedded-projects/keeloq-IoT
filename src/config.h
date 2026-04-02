#pragma once

#include <Arduino.h>
#include <vector>
#include <keeloq.h>

// piny do czujników stanu bramy
#define OPEN_SENSOR    27
#define OPENING_SENSOR  26
#define CLOSED_SENSOR  25
#define CLOSING_SENSOR  32

// pin do otwierania/zamykania bramy
#define GATE_PIN       14

// dane do połączenia z brokerem MQTT
constexpr const char* WIFI_SSID     = "";
constexpr const char* WIFI_PASSWORD = "";
constexpr const char* AIO_SERVER   = "io.adafruit.com";
constexpr const int   AIO_PORT     = 8883;
constexpr const char* AIO_USERNAME = "";
constexpr const char* AIO_KEY      = "";
constexpr const char* clientId     = "ESP32-Client";
constexpr const char* AIO_FEED     = "gateway";

// Root CA dla io.adafruit.com (Let's Encrypt R3)
static const char* AIO_ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEjTCCA3WgAwIBAgIQDQd4KhM/xvmlcpbhMf/ReTANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xNzExMDIxMjIzMzdaFw0yNzExMDIxMjIzMzdaMGAxCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xHzAdBgNVBAMTFkdlb1RydXN0IFRMUyBSU0EgQ0EgRzEwggEiMA0GCSqGSIb3
DQEBAQUAA4IBDwAwggEKAoIBAQC+F+jsvikKy/65LWEx/TMkCDIuWegh1Ngwvm4Q
yISgP7oU5d79eoySG3vOhC3w/3jEMuipoH1fBtp7m0tTpsYbAhch4XA7rfuD6whU
gajeErLVxoiWMPkC/DnUvbgi74BJmdBiuGHQSd7LwsuXpTEGG9fYXcbTVN5SATYq
DfbexbYxTMwVJWoVb6lrBEgM3gBBqiiAiy800xu1Nq07JdCIQkBsNpFtZbIZhsDS
fzlGWP4wEmBQ3O67c+ZXkFr2DcrXBEtHam80Gp2SNhou2U5U7UesDL/xgLK6/0d7
6TnEVMSUVJkZ8VeZr+IUIlvoLrtjLbqugb0T3OYXW+CQU0kBAgMBAAGjggFAMIIB
PDAdBgNVHQ4EFgQUlE/UXYvkpOKmgP792PkA76O+AlcwHwYDVR0jBBgwFoAUTiJU
IBiV5uNu5g/6+rkS7QYXjzkwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsG
AQUFBwMBBggrBgEFBQcDAjASBgNVHRMBAf8ECDAGAQH/AgEAMDQGCCsGAQUFBwEB
BCgwJjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29tMEIGA1Ud
HwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEds
b2JhbFJvb3RHMi5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEW
HGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwDQYJKoZIhvcNAQELBQADggEB
AIIcBDqC6cWpyGUSXAjjAcYwsK4iiGF7KweG97i1RJz1kwZhRoo6orU1JtBYnjzB
c4+/sXmnHJk3mlPyL1xuIAt9sMeC7+vreRIF5wFBC0MCN5sbHwhNN1JzKbifNeP5
ozpZdQFmkCo+neBiKR6HqIA+LMTMCMMuv2khGGuPHmtDze4GmEGZtYLyF8EQpa5Y
jPuV6k2Cr/N3XxFpT3hRpt/3usU/Zb9wfKPtWpoznZ4/44c1p9rzFcZYrWkj3A+7
TNBJE0GmP2fhXhP1D/XVfIW/h0yCJGEiV9Glm/uGOa3DXHlmbAcxSyCRraG+ZBkA
7h4SeM6Y8l/7MBRpPCz6l8Y=
-----END CERTIFICATE-----
)EOF";


// === KONFIGURACJA CC1101 ===
#define CC1101_CS_PIN    5
#define CC1101_IRQ_PIN   15
#define CC1101_RST_PIN   4

// DeviceKey'e dla wszystkich pilotów
inline constexpr uint64_t MY_KEY = 0x0;
inline constexpr uint64_t WHITE_KEY = 0x0;
inline constexpr uint64_t BLACK_KEY = 0x0;
inline constexpr uint64_t BLUE_KEY = 0x0;

inline Remote remote1 = {.serial = 0, .deviceKey = MY_KEY, .counter = 0, .disc = 0};
inline Remote remote2 = {.serial = 0, .deviceKey = WHITE_KEY, .counter = 0, .disc = 0};
inline Remote remote3 = {.serial = 0, .deviceKey = BLACK_KEY, .counter = 0, .disc = 0};
inline Remote remote4 = {.serial = 0, .deviceKey = BLUE_KEY, .counter = 0, .disc = 0};

inline std::vector<Remote> remotes = {remote1, remote2, remote3, remote4};