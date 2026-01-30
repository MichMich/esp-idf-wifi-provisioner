# Basic Example

Minimal example that connects to a stored WiFi network or starts a captive portal for provisioning.

## What It Does

1. Initialises the WiFi provisioner with default settings
2. If stored credentials exist, attempts a STA connection
3. Otherwise starts a soft-AP (`MyDevice-Setup`) with a captive portal
4. Blocks until a WiFi connection is established

## Build & Flash

```bash
cd examples/basic
idf.py set-target esp32s3   # or esp32, esp32c3, etc.
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

Run `idf.py menuconfig` and navigate to **Component config > WiFi Provisioner** to change:

| Option | Default | Description |
|---|---|---|
| AP SSID | `ESP-Provision` | Name of the provisioning access point |
| AP password | *(empty)* | Leave empty for an open AP |
| AP channel | `1` | WiFi channel |
| Max connections | `4` | Simultaneous AP clients |
| STA max retries | `5` | Connection attempts before fallback |
| Portal timeout | `180` s | Auto-shutdown (0 = disabled) |
| HTTP port | `80` | Captive portal web server port |
| Page title | `WiFi Setup` | Browser tab title |
| Portal header | `WiFi Setup` | Main heading on the portal page |
| Portal subheader | `Please connect to your WiFi network.` | Text below the portal heading |
| Connected header | `Saved!` | Heading on the confirmation page |
| Connected subheader | `Connecting to the network. You can close this page.` | Text below the confirmation heading |
| Page footer | `&copy; 2026` | Footer on both pages (HTML supported) |

All options can also be overridden at runtime via `wifi_prov_config_t`. See `main.c` for an example.

## Erasing Stored Credentials

To force the portal to appear again:

```bash
idf.py -p /dev/ttyUSB0 erase-flash
```

Or call `wifi_prov_erase_credentials()` in your application code.
