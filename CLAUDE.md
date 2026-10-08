# SmartKnob firmware

- Building, flashing, backing up and checking the version of a knob from the
  command line: [docs/flashing.md](docs/flashing.md). Use the `seedlabs_devkit`
  environment, and never flash the filesystem without carrying `config.pb` and
  `settings.pb` over — that wipes the knob's calibration.
- The knob talks to its controller over MQTT (`firmware/src/network/mqtt_task.cpp`).
  The protocol says "hass" but works with any controller that speaks it.
