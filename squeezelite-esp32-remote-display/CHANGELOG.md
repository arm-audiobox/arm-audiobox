# Changelog

## 0.13.1 — approved display build

- Daily night dimming with persistent enabled state, start/end times and brightness. Defaults: 22:00–07:00, 10%.
- Start/end editing with separate hour/minute up/down controls and repeated steps while held. Save confirms; Back or timeout cancels unconfirmed changes.
- Restore normal brightness outside the interval; handle overnight intervals and local daylight-saving time.
- Keep the exact tested application identifier `0.13.1-overlay-test` in the approved binary.

## 0.12 series — overlay and display controls

- Convert operating menus to overlays; keep provisioning and calibration full-screen.
- Draw text at native resolution, with brighter secondary labels and wrapping/truncation.
- Move all Back controls to the right and adjust touch targets accordingly.
- Add persistent display brightness and live preview.
- Render menus in small strips to retain memory for LMS responses.

## 0.11

- Header overlays for player selection, volume and Wi-Fi status.
- Replace double arrows with previous/next track arrows and remove bottom volume buttons.
