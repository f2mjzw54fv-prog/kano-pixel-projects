# kano-pixel-projects

Online project slots for the Kano Pixel Kit. The Kano's firmware keeps
**selection 1** as the built-in stock ticker; **selections 2-4** are
fetched live from this repo over WiFi:

- `slots/slot2.json` → Kano selection 2
- `slots/slot3.json` → Kano selection 3
- `slots/slot4.json` → Kano selection 4

To put something new on the Kano, just ask Muse, e.g.
"make slot 2 a spooky Halloween pumpkin" or
"make slot 3 scroll GO DODGERS in blue".
The Kano picks the new project up within a few minutes —
no cable, no re-flashing.

## Project format (v1)

```json
{
  "name": "Go Dodgers",
  "version": 1,
  "type": "scroll",
  "text": "GO DODGERS",
  "color": "0055FF",
  "bg": "000000",
  "speed": 40
}
```

Types:

- `scroll` — scrolling text. `text`, `color`, `bg` (hex `RRGGBB`),
  `speed` = ms per scroll step.
- `static` — centered still text. `text`, `color`, `bg`.
- `effect` — built-in animation. `effect` is one of
  `rainbow`, `plasma`, `sparkle`. `speed` = ms per frame.
- `frames` — custom pixel animation. `frames` is an array of
  `{"hold": ms, "pixels": ["RRGGBB", ...128 entries...]}`,
  row-major, 16 wide × 8 tall, top-left first.

Notes:

- Text uses a 5×7 font; supported characters are
  `A-Z 0-9` and `space + - . %`.
- The Kano's physical dial always controls brightness.
- The Kano re-downloads the slot file every 5 minutes while
  that slot is selected, so edits go live on their own.

## Firmware

`firmware/kano_pixel_slots.ino` — the Arduino sketch that powers
the slot system. Needs the ESP32 board package, FastLED, and
ArduinoJson. Set `WIFI_SSID` / `WIFI_PASS` before flashing.
