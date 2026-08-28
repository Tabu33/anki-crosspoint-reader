# Anki Review for CrossPoint Reader

Anki flashcard review on the Xteink X3/X4 e-reader, built on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) firmware. Cards sync through [AnkiConnect](https://foosoft.net/projects/anki-connect/)

## Quick start

1. In desktop Anki: Tools → Add-ons → Get Add-ons → code `2055492159` → restart.

2. Tools → Add-ons → AnkiConnect → Config:
```json
   { "apiKey": null, "webBindAddress": "0.0.0.0", "webBindPort": 8765 }
```
   Restart Anki. Allow port 8765 through your firewall.

3. Find your computer's local IP (`ipconfig` / `ifconfig`).

4. Download `firmware.bin` from [Releases](../../releases/latest).

5. Flash it: go to [crosspointreader.com/#flash-tools](https://crosspointreader.com/#flash-tools) select your device, then  click **Custom .bin**, upload `firmware.bin`, click **Flash**, and keep the device connected until it finishes.

6. On the device: Home → Anki → Server URL `http://<your IP>:8765` → Start Review.

## Configuring the device

On the device: **Home → Anki**.

- **AnkiConnect Server URL** — `http://<your computer's local IP>:8765`.
- **API Key**- leave blank.
- **Deck** - optional, restricts review to a single deck, fetched live from Anki.
- **Start Review** - front, flip to back, grade with Again / Hard / Good / Easy.

Settings are saved to the device and persist across reboots.

## Building from source

```bash
git clone --recursive https://github.com/crosspoint-reader/crosspoint-reader
cd crosspoint-reader
cp -r /path/to/firmware-patch/* .
pip install platformio
python -m platformio run -e default -t upload
```

## Compatibility

Built and tested on the Xteink X4 (ESP32-C3, the `default` build environment). 

## Limitations

- No images on cards.
- Currently only supports English

## License and attribution

Built on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader). Not affiliated with Xteink or Anki.
