# Security Policy and Known Considerations

Sentry AI is a hackathon prototype. It is published as an engineering portfolio artefact and is **not suitable for deployment in a live safety-critical environment** without the remediation described below. Documenting these issues openly is intentional: a safety device whose weaknesses are unknown is more dangerous than one whose weaknesses are catalogued.

## Reporting a vulnerability

Please open a GitHub issue describing the problem and the conditions required to reproduce it. Do not include real credentials, worker identities or site coordinates in an issue.

## Credential handling

Wi-Fi Access Point credentials are **not** stored in tracked source. `firmware/src/config.example.h` contains placeholders only; the real `config.h` is excluded by `.gitignore` and must be created locally by each developer.

An earlier revision of this project distributed the firmware inside a Word document that contained a hardcoded SoftAP password. Any credential from that revision must be treated as compromised and rotated on every device.

## Known issues in the current firmware

| # | Issue | Impact | Suggested remediation |
|---|-------|--------|----------------------|
| 1 | `/reset` and `/clearsession` are unauthenticated state-changing `GET` endpoints | Any client on the Access Point can silence an active fall alarm or erase incident history. Because they are `GET`, a page a supervisor happens to open could trigger them cross-site | Require `POST` with a session token or CSRF token; add operator authentication |
| 2 | `Access-Control-Allow-Origin: *` on `/data` | Any web origin loaded by a connected client can read live worker telemetry, including coordinates | Restrict to a known origin, or remove the header if no browser-based cross-origin client is needed |
| 3 | Plain HTTP transport | Telemetry and control traffic are unencrypted and can be observed or modified by anyone within radio range who has the AP password | Terminate TLS on the device, or tunnel through an authenticated gateway |
| 4 | Single shared SoftAP password | The same secret grants access to every function; there is no separation between a supervisor and a passer-by who learned the password | Per-device credentials with a rotation procedure; role separation for control endpoints |
| 5 | Worker identity compiled into firmware | `WORKER_ID` and `WORKER_NAME` are build-time constants, so reassigning a helmet requires a reflash and identity is embedded in the binary | Move to runtime provisioning stored in NVS |
| 6 | No integrity protection on stored events | The NVS event queue can be cleared through `/clearsession` without authentication, so incident records are not tamper-evident | Append-only log with sequence numbers and authenticated erase |
| 7 | Alert delivery is proximity-bound | Alerts reach only clients associated with that helmet's Access Point. A worker who falls alone with no supervisor in range produces no remote notification | Add an uplink path such as LoRa or cellular, as noted in the project roadmap |

## Privacy considerations

The device continuously records location and derived behavioural signals such as an inferred fatigue score. Any real deployment should define a retention period, restrict who may view historical worker movement, and disclose the monitoring to the workers concerned. Hazard-zone clustering stores coordinates where falls occurred, which is site data rather than personal data, but the two become linkable once combined with worker identifiers.

## Scope

This policy covers the firmware and documentation in this repository. It does not cover the physical helmet shell, which is a certified item of personal protective equipment in its own right and must not be modified in any way that compromises its impact rating.
