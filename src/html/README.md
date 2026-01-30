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
	{ "ssid": "My Network", "rssi": -42, "auth": 3 },
	{ "ssid": "Open WiFi", "rssi": -75, "auth": 0 }
]
```

- `auth: 0` = open network (no lock icon)
- `auth: >0` = password protected (lock icon)

The `scan/` directory is in `.gitignore` and will not be committed.

## Dummy config data

The `config/` directory contains an `index.html` with mock page configuration. When the portal page fetches `/config`, the local server resolves it to `config/index.html`.

Edit `config/index.html` to change the test data:

```json
{
	"title": "WiFi Setup",
	"portal_header": "WiFi Setup",
	"portal_subheader": "Select your network to get started.",
	"connected_header": "Saved!",
	"connected_subheader": "Connecting to the network. You can close this page.",
	"footer": "&copy; 2026"
}
```

## Dummy save response

The `save/` directory contains an `index.html` with a mock save response. When the portal submits credentials via `fetch('/save', ...)`, the local server resolves it to `save/index.html`.

## Pages

- **portal.html** — WiFi setup page (network list, credential form, inline connection feedback)
