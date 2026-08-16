# M5ChainOSC Device Presets

[日本語版](README.md)

These are reusable configuration examples for individual M5ChainOSC Chain devices.

> [!IMPORTANT]
> M5ChainOSC is an unofficial, independently developed project. It is not an official product of M5Stack Technology Co., Ltd. and is not affiliated with or endorsed by that company.

Presets contain neither a UID nor Device Name, so they can be imported into another Chain device of the same type. New users can follow the [Preset Quick Start](https://shimez.github.io/M5ChainOSC/en/quick-start-presets/).

## How to use a preset

1. Open a JSON file from the list below.
2. Select GitHub's `Download raw file` button.
3. Open the M5ChainOSC Web UI.
4. Open `…` on the target device.
5. Select `Import Preset (JSON)`.
6. Choose the downloaded file and confirm the import.
7. Adjust OSC addresses, axes, or inversion settings as needed.

> [!IMPORTANT]
> A preset can only be imported into the same device type. A Key preset cannot be imported into a Joystick or Encoder.

## Presets

### Joystick

#### [VRChat Move](joystick/vrchat-move.json)

Move in VRChat with the stick and jump by pressing it. Adjust X/Y assignments, OSC addresses, and axis inversion to match the device orientation.

### Key

#### [VRChat AFK Control](key/vrchat-afk-control.json)

Toggle VRChat AFK mode.

#### [VRChat Voice Control](key/vrchat-voice-control.json)

Toggle the VRChat microphone.

#### [VRChat Camera Controls](key/vrchat-camera-controls.json)

Change camera mode, Spout, and Smooth settings together for streaming.

### Angle

#### [VRChat Camera Zoom](angle/vrchat-camerazoom.json)

Adjust the VRChat camera zoom with Angle.

### Encoder

#### [VRChat Camera Zoom](encoder/vrchat-camerazoom.json)

Adjust the VRChat camera zoom with Encoder rotation.

## Notes

- Import does not change the target UID or Device Name.
- Import replaces that device's OSC and mode settings.
- Export the current device with `Export Preset (JSON)` first if you may need to restore it.
- Use `Settings Backup & Restore` for migration or recovery of the complete device configuration.
- OSC addresses may require adjustment for the VRChat avatar or environment.
- Do not change `format`, `schemaVersion`, or `deviceType` when editing JSON manually.

See the [Settings JSON schema](../SETTINGS_SCHEMA.md#デバイス設定プリセット) for the preset format.
