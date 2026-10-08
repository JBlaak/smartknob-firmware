# Flashing the knob from the command line

How to build and flash the Seedlabs DevKit (ESP32-S3, 16 MB) with the PlatformIO
CLI, without VS Code, and without losing the knob's calibration and settings.
Everything here runs on macOS; Linux works the same with a different port name.

## Setup

```sh
brew install platformio mtools
```

PlatformIO brings its own esptool, used below as:

```sh
ESPTOOL="pio pkg exec -p tool-esptoolpy -- esptool.py"
```

Find the knob's port. Its name depends on the USB socket it's in
(`/dev/cu.usbmodem2101`, `/dev/cu.usbmodem101`, …):

```sh
ls /dev/cu.usbmodem*
PORT=/dev/cu.usbmodem2101
```

Build with the `seedlabs_devkit` environment. `seedlabs_devkit_github_action_release`
is what CI uses for releases, and it needs SeedLabs' private `skdk-ota-pro` repo.

## What is on the knob

| Partition | Offset | Size | Contents |
|---|---|---|---|
| nvs | `0x9000` | `0x5000` | Wi-Fi and MQTT credentials (the firmware's "EEPROM") |
| otadata | `0xe000` | `0x2000` | which app slot boots |
| ota_0 | `0x10000` | `0x600000` | firmware (`pio … -t upload` writes here) |
| ota_1 | `0x610000` | `0x600000` | firmware, second slot |
| ffat | `0xc50000` | `0x100000` | FAT: `config.pb` (motor + strain calibration), `settings.pb`, and the `setup/` web pages |

Flashing the **firmware** leaves nvs and ffat alone. Flashing the **filesystem**
replaces all of ffat, calibration included, unless you carry `config.pb` and
`settings.pb` over (below).

## Which version runs on it

Read both app slots and look for the version, which sits next to the
`firmware_version` key it is sent under. Local builds report `DEV`.

```sh
$ESPTOOL --port $PORT -b 921600 read_flash 0x10000 0x600000 /tmp/ota_0.bin
$ESPTOOL --port $PORT -b 921600 read_flash 0x610000 0x600000 /tmp/ota_1.bin
strings -n 3 /tmp/ota_0.bin | grep -B1 -x firmware_version | head -1
strings -n 3 /tmp/ota_1.bin | grep -B1 -x firmware_version | head -1
```

## Back up first

```sh
mkdir -p ~/smartknob-backups && cd ~/smartknob-backups
$ESPTOOL --port $PORT -b 921600 read_flash 0x0      0x10000   boot.bin
$ESPTOOL --port $PORT -b 921600 read_flash 0x9000   0x5000    nvs.bin
$ESPTOOL --port $PORT -b 921600 read_flash 0x10000  0x600000  ota_0.bin
$ESPTOOL --port $PORT -b 921600 read_flash 0x610000 0x600000  ota_1.bin
$ESPTOOL --port $PORT -b 921600 read_flash 0xc50000 0x100000  ffat.bin
```

`nvs.bin` holds the Wi-Fi and MQTT passwords in plain text; keep the backups to
yourself.

## Flash the firmware

```sh
pio run -e seedlabs_devkit -t upload --upload-port $PORT
```

This is all a code change needs. After pulling in someone else's work, check
first that the partition table is still the one on the knob:

```sh
$ESPTOOL --port $PORT read_flash 0x8000 0xc00 /tmp/partitions.bin
cmp .pio/build/seedlabs_devkit/partitions.bin /tmp/partitions.bin && echo same
```

## Flash the filesystem, keeping calibration and settings

Only needed when `firmware/data/` changed (the setup web pages), e.g. when
moving between releases. Pull the two config files out of the knob's FAT
partition, build the image with them in it, flash, and clean up:

```sh
$ESPTOOL --port $PORT -b 921600 read_flash 0xc50000 0x100000 /tmp/ffat.bin
MTOOLS_SKIP_CHECK=1 mcopy -n -i /tmp/ffat.bin ::/config.pb ::/settings.pb firmware/data/

pio run -e seedlabs_devkit -t buildfs
MTOOLS_SKIP_CHECK=1 mdir -i .pio/build/seedlabs_devkit/fatfs.bin ::/   # expect config.pb, settings.pb, setup
pio run -e seedlabs_devkit -t uploadfs --upload-port $PORT

rm firmware/data/config.pb firmware/data/settings.pb
```

The config format is versioned (`PERSISTENT_CONFIGURATION_VERSION` and
`SETTINGS_VERSION` in `firmware/src/configuration.h`). If a release bumps them,
the old files are rejected and the knob asks to be set up again.

`uploadfs` writes at `0xc51000`, one sector into the partition, and prints
`failed to open "/fatfs/setup" for writing` while building. Both are normal.

## Serial console

```sh
pio device monitor -p $PORT -b 115200
```

Keys the firmware listens for: `V` toggles verbose logging, `O` shows where each
log line comes from, and `c` recalibrates the motor. **Keep your hands off the
knob during calibration**; the motor turns it for about 20 seconds and saves the
result to `config.pb`.

Opening the port doesn't reset the knob. Any esptool command resets it when it
is done, so the quickest reboot from the Mac is `$ESPTOOL --port $PORT read_mac`.

## Pointing the knob at another MQTT broker

Without going through onboarding again: the knob's web server takes new MQTT
credentials while it is on Wi-Fi. It answers once it has tried them.

```sh
curl -X POST -H 'Content-Type: application/json' \
  -d '{"server":"10.10.0.20","portInt":1883,"username":"…","password":"…"}' \
  http://<knob-ip>/mqtt
```

## When it misbehaves

- **It resets when you press it, and its USB port disappears.** Check the back:
  pushing the knob down on a desk can press the EN/RST button. Hold it in your
  hand, or raise it, and try again before suspecting the firmware.
- **The port isn't there at all.** Hold BOOT and EN/RST on the back, let go of
  EN/RST, then BOOT: the knob is now in download mode.
- **The dial sticks in the menu.** 0.5.2 runs the motor task at priority 0 and
  it starves under MQTT load; `main` has it back at 1.
