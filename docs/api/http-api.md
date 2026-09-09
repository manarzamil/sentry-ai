# HTTP API

The ESP32 hosts a `WebServer` on port 80, reachable over the Wi-Fi Access Point that the helmet broadcasts. The ESP32 SoftAP default address is `192.168.4.1`; the firmware prints the actual value to the serial console at boot.

## Endpoints

| Method | Path | Response | Purpose |
|--------|------|----------|---------|
| GET | `/` | `text/html` | Human-readable dashboard, self-refreshing every 5 seconds |
| GET | `/data` | `application/json` | Complete machine-readable telemetry snapshot |
| GET | `/reset` | 303 redirect to `/` | Clears the latched fall, SOS and fatigue alerts and silences the buzzer |
| GET | `/clearsession` | 303 redirect to `/` | Wipes persisted counters, hazard zones and the alert log |

## Telemetry payload

`GET /data` returns a flat JSON object assembled in `handleData()`.

| Field | Type | Description |
|-------|------|-------------|
| `worker_id` | string | Identifier compiled into the firmware |
| `worker_name` | string | Display name compiled into the firmware |
| `system_active` | boolean | Whether the master switch has enabled monitoring |
| `ax`, `ay`, `az` | number | Per-axis acceleration in m/s² |
| `mag` | number | Acceleration magnitude in m/s² |
| `tilt` | number | Tilt from vertical in degrees |
| `gx`, `gy`, `gz` | number | Angular rate per axis in degrees per second |
| `lat`, `lng` | number | Last valid coordinates, six decimal places |
| `gps_valid` | boolean | A fix newer than 2000 ms is available |
| `gps_cached` | boolean | No live fix, but previously stored coordinates exist |
| `gps_lost_s` | number | Seconds since the fix was lost |
| `fall` | boolean | Latched fall alert |
| `fall_phase` | number | Current state machine phase, 0–3 |
| `sos` | boolean | Latched manual SOS |
| `fatigue` | boolean | Latched fatigue alert |
| `fat_score` | number | Current fatigue score, 0–100 |
| `fall_cnt` | number | Falls recorded this session |
| `sos_cnt` | number | SOS presses recorded this session |
| `offline_q` | number | Events buffered in non-volatile storage |
| `helmet_worn` | boolean | Helmet-worn heuristic result |
| `uptime_s` | number | Seconds since monitoring was enabled |
| `log` | string | Up to 9 recent events, delimited by `||` |
| `zones` | array | Hazard zones as `{n, lat, lng, c}` |

## Response headers

`/data` is served with `Access-Control-Allow-Origin: *` and `Cache-Control: no-cache`. The wildcard CORS policy allows any web origin loaded in a connected client's browser to read helmet telemetry. See `../../SECURITY.md`.

## Integration example

```bash
curl http://192.168.4.1/data
```

```javascript
const res = await fetch('http://192.168.4.1/data');
const t = await res.json();
if (t.fall) {
  console.warn(\`Fall reported for \${t.worker_id} at \${t.lat}, \${t.lng}\`);
}
```

## Known interface limitations

`/reset` and `/clearsession` change device state but are exposed as unauthenticated `GET` requests, which makes them reachable by any client on the Access Point and triggerable by a simple embedded resource. Transport is plain HTTP with no TLS. Both issues are recorded in the project threat model.
