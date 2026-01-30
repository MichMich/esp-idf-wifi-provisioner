# Testing the portal HTML locally

You can preview and edit the captive portal pages in a browser without flashing an ESP32.

## Setup

Start a local HTTP server from this directory:

```bash
cd src/html
serve
```

Then open the URL shown in the terminal (typically http://localhost:3000/portal.html).

## Dummy scan data

The `scan/` directory contains an `index.html` with mock JSON data. When the portal page fetches `/scan`, the local server resolves it to `scan/index.html` and returns the dummy network list.

Edit `scan/index.html` to change the test data:

```json
[
    {"ssid": "My Network", "rssi": -42, "auth": 3},
    {"ssid": "Open WiFi", "rssi": -75, "auth": 0}
]
```

- `auth: 0` = open network (no lock icon)
- `auth: >0` = password protected (lock icon)

The `scan/` directory is in `.gitignore` and will not be committed.

## Pages

- **portal.html** — Main WiFi setup page (network list, SSID/password form)
- **connected.html** — Confirmation shown after saving credentials
