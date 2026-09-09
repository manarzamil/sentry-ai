# Dashboard Screenshots

This directory holds captures of the ESP32-served dashboard and telemetry endpoint.

**Status: not yet captured.** No dashboard screenshot exists at the time of writing, and none has been fabricated. The images below will be added after hardware testing.

Planned captures:

- `dashboard-safe-state.png` — dashboard in the normal monitoring state
- `dashboard-fall-alert.png` — dashboard during a latched fall alert
- `dashboard-fatigue-warning.png` — dashboard showing an active fatigue warning
- `data-json-payload.png` — raw response from the `GET /data` endpoint
- `serial-fall-confirmed.png` — serial monitor showing the `[FALL] CONFIRMED` sequence

Capture guidance: connect a phone or laptop to the helmet Access Point, open the dashboard at the address printed on the serial monitor, and capture at a width of at least 1080 px. Do not crop out the status header, as it carries the system state, worn state and GPS state.
