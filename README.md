# Xiaomi Light Bar — ESPHome external component

ESPHome port of [ebinf/lightbar2mqtt](https://github.com/ebinf/lightbar2mqtt).
Control a Xiaomi Mi Computer Monitor Light Bar (model **MJGJD01YL**, the one
without BLE/WiFi) from Home Assistant via an ESP32 + nRF24L01, with no MQTT
broker required — entities go straight over ESPHome's native API.

The on-air protocol implementation is a port of the original C++ code, which in
turn is based on the reverse-engineering work by
[lamperez/xiaomi-lightbar-nrf24](https://github.com/lamperez/xiaomi-lightbar-nrf24).

## What you get

- A `light` entity per bar, with:
  - On/off, brightness (0–15 internally, exposed as 0–100% to HA), and color
    temperature (153–370 mireds).
- A `text_sensor` entity per observed remote, publishing the action string
  (`press`, `turn_clockwise`, `turn_counterclockwise`, `hold`,
  `press_turn_clockwise`, `press_turn_counterclockwise`).
- Optional pair action callable from a template button (see `example.yaml`).

## Hardware

| nRF24 | ESP32  |
| :---- | -----: |
| VCC   |    3V3 |
| GND   |    GND |
| CE    |  GPIO4 |
| CSN   |  GPIO5 |
| SCK   | GPIO18 |
| MOSI  | GPIO23 |
| MISO  | GPIO19 |

The SCK/MOSI/MISO pins are the ESP32 default hardware SPI bus; the CE/CSN pins
can be any GPIO and are configured in YAML.

## Configuration

```yaml
external_components:
  - source:
      type: local      # or `git` pointing at this repo
      path: components
    components: [xiaomi_lightbar]

spi:
  clk_pin: GPIO18
  miso_pin: GPIO19
  mosi_pin: GPIO23

xiaomi_lightbar:
  id: bar_hub
  cs_pin: GPIO5
  ce_pin: GPIO4

light:
  - platform: xiaomi_lightbar
    id: living_room_bar
    name: "Living room bar"
    hub_id: bar_hub
    serial: 0xABCDEF

text_sensor:
  - platform: xiaomi_lightbar
    name: "Bar remote"
    hub_id: bar_hub
    serial: 0x123456
```

See [`example.yaml`](./example.yaml) for a complete config including the
optional pair button.

## Finding your remote's serial

Flash with no `text_sensor` entries (or with a placeholder serial), press a
button on the physical remote, and look at the ESPHome logs:

```
[D][xiaomi_lightbar]: RX from unknown serial 0x7B7E12 (cmd=0x01)
```

Put that into the YAML and re-flash.

## Pairing

Choose any serial for the light bar in YAML (e.g. `0xABCDEF`), then:

1. Power-cycle the light bar.
2. Within 10 seconds, fire the pair button (see `example.yaml`):
   ```yaml
   button:
     - platform: template
       name: "Pair light bar"
       on_press:
         - lambda: 'id(living_room_bar_output).send_pair();'
   ```
3. The bar should blink a few times to confirm pairing.

If you instead want to use the same serial as the original remote (so both
control the same bar without re-pairing), set the light's `serial:` to that
remote's serial. You don't need to pair in that case.

## Known limitations

Inherited from the underlying one-way protocol:

- No feedback from the bar. State is tracked locally on the ESP32; if you
  toggle the bar via the original remote, ESPHome's view will desync until you
  command it again from Home Assistant.
- Boot assumes the bar is on. Toggle from HA + power-cycle the bar to resync.
- Some remote presses are occasionally missed at the radio layer. Don't rely
  on 100% delivery for automations — pair the bar with the ESP32 (different
  serial than the remote) if you want HA to see every event.

## License

MIT, like the upstream project.
