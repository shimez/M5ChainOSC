---
layout: default
title: M5ChainOSC User Guide
permalink: /en/user-guide/
---

# M5ChainOSC User Guide

[日本語版](../../user-guide/)

This guide explains initial setup after installing M5ChainOSC on an AtomS3R and how to use the Web UI.

> [!IMPORTANT]
> M5ChainOSC is an unofficial, independently developed project. It is not an official product of M5Stack Technology Co., Ltd. and is not affiliated with or endorsed by that company.

## 1. Initial Wi-Fi setup

When no Wi-Fi settings are stored, the AtomS3R starts in access point mode.

> [!IMPORTANT]
> The AtomS3R supports **2.4 GHz Wi-Fi only**. It cannot connect to a 5 GHz-only SSID.

1. Open Wi-Fi settings on a phone or computer.
2. Connect to the SSID `AtomS3R-OSC`.
3. Enter the password `12345678`.
4. If the setup page does not open automatically, visit `http://192.168.4.1/`.
5. Enter the SSID and password of the Wi-Fi network the AtomS3R should use, then save.
6. The AtomS3R restarts. After a successful connection, its display shows `WiFi OK` and its IP address.

If connection fails, the device returns to access point mode. Check the SSID, password, and whether the selected network supports 2.4 GHz.

## 2. Open the settings page

From a device on the same network as the AtomS3R, open either:

- The IP address shown on the AtomS3R display
- `http://atoms3r-osc.local/`

Some environments do not support mDNS `.local` addresses. Use the displayed IP address in that case.

## 3. Common settings

### Language

Select `English` or `日本語`. The page reloads immediately and stores the selection on the AtomS3R. Before a language has been saved, the Web UI uses Japanese when it is the browser's preferred language and English otherwise.

### WiFi

This section shows the current IP address. `Delete WiFi Settings` removes the saved Wi-Fi configuration; the device starts in access point mode on its next boot.

### Settings Backup / Restore Settings

- `Export Settings (JSON)`: downloads all saved settings as JSON.
- `Import Settings (JSON)`: restores settings from an exported JSON file.

Import replaces settings for matching device UIDs while retaining saved devices not present in the file. OSC destination, display rotation, and Web UI language are restored immediately. Wi-Fi credentials are never included.

### Display Rotation

Select `0°`, `90°`, `180°`, or `270°`. The AtomS3R display changes immediately.

### OSC Destination

- `Host IP`: IP address of the computer or device running the OSC receiver
- `Port`: OSC receiving port

VRChat normally receives OSC on port `9000`. Use the settings required by your receiving application.

### Enable OSC in VRChat

To send OSC from M5ChainOSC to VRChat, enable OSC from the VRChat Action Menu:

```text
Action Menu → Options → OSC → Enabled
```

## 4. Common device operations

Each connected Chain device appears as a card.

- `Device Name`: an optional name used for identification and status display
- UID: the device's unique identifier; settings are stored per UID
- `Save All Settings`: saves the OSC destination and all connected device settings

Recognized devices normally show a blue LED. Open `…` on a device and select `Identify Device (Orange LED for 10s)` to turn only that device orange for ten seconds. It then returns to blue. Identification is not stored as a setting.

Always press `Save All Settings` after editing. Saved settings are restored when the same UID reconnects, even after removal or reboot.

### Saved Device Settings

Lists devices that have been saved, whether currently connected or not. `Delete Settings` removes the settings for that UID. If the device is connected, its configuration returns to defaults.

## 5. OSC message input rules

### OSC Address

- Must start with `/`.
- Maximum length is 192 bytes.
- Spaces and `# * , ? [ ] { }` are not allowed.

Example:

```text
/avatar/parameters/Flying
```

### Type and Value

- `Float`: examples include `0.0`, `1.0`, and `-0.5`
- `Int`: examples include `0`, `1`, and `-10`
- `String`: any text value

Value is limited to 128 bytes. The byte count and validation message appear below each field.

## 6. Key

A Key sends OSC messages when pressed or released.

| Parameter | Meaning |
| --- | --- |
| `Device Name` | Optional name used to identify the device. |
| `Key Mode` | Selects `Press / Release` or `Sequence`. |

### Press / Release mode

Messages are sent from top to bottom for each event.

- `Press`: messages sent when the key is pressed
- `Release`: messages sent when the key is released
- `OSC Address`: destination of the message
- `Type`: type of the value
- `Value`: value to send
- Press and Release can contain up to eight messages in total.
- Use `+ Add OSC Message` to add a message.
- Use the arrow buttons to reorder messages.
- Use `Delete` to remove a message.
- An event containing zero messages sends nothing.

### Sequence mode

Each press sends the next value to the same OSC Address.

| Parameter | Meaning |
| --- | --- |
| `OSC Address` | Destination of the sequence value. |
| `Start` | Value sent on the first press and after wrapping. |
| `End` | End of the sequence. |
| `Step` | Amount added on each press. A zero step is corrected to `1`; direction is corrected when Start is greater than End. |
| `Type` | Use `Float` or `Int`. Int values are sent as integers. |

After sending Start, each press advances by Step. When the next value would pass End, the sequence returns to Start. Release sends nothing. Saving resets the next value to Start.

## 7. Encoder

### Encoder Rotation

| Parameter | Meaning |
| --- | --- |
| `Rotation Address` | OSC destination for rotation values. |
| `Mode` | `Absolute` uses the current counter position; `Increment` uses the change since the previous reading. |
| `Abs In Min` | Input position mapped to Out Min in Absolute mode. Hidden in Increment mode. |
| `Abs In Max` | End of the repeating Absolute input range. Hidden in Increment mode. |
| `Inc Scale` | Multiplier applied to each Increment delta. A delta of `1` with `0.05` sends `0.05`; reverse rotation produces a negative value. |
| `Out Min` | Lower output limit. |
| `Out Max` | Upper output limit. |
| `Out Type` | `Float`, rounded `Int`, or numeric `String`. |

#### Absolute input range

Unlike a potentiometer, an encoder has no physical minimum or maximum and can rotate continuously. Absolute mode treats a fixed number of internal encoder counts as one cycle and maps the current position to the output range.

Example:

```text
Abs In Min: 0
Abs In Max: 20
Out Min: 0
Out Max: 1
```

| Encoder count | OSC output |
| ---: | ---: |
| `0` | `0.00` |
| `5` | `0.25` |
| `10` | `0.50` |
| `15` | `0.75` |
| `19` | `0.95` |
| `20` | wraps to `0.00` |

This configuration completes one output cycle every 20 counts. Reverse rotation wraps from count `0` to the end of the range.

It is usually easiest to keep `Abs In Min` at `0` and treat `Abs In Max` as the number of counts per cycle:

- `10`: faster change with fewer turns
- `20`: medium adjustment
- `100`: finer adjustment per count

The current implementation wraps when it reaches Abs In Max. With integer encoder counts, it therefore approaches Out Max and then returns to Out Min without sending Out Max itself. In the example above, the highest value is `0.95`.

The first reading initializes the position. OSC is sent when the encoder value changes.

### Click

The green-accented section configures the encoder button and works like Key.

- Select `Press / Release` or `Sequence (press only)`.
- Press and Release can contain up to eight messages in total.
- Messages can be added, deleted, and reordered.
- An event with zero messages sends nothing.
- Sequence fields have the same meanings as Key Sequence.

Rotation and click settings are independent.

## 8. Joystick

### Joystick XY

| Parameter | Meaning |
| --- | --- |
| `X Address` | OSC destination for the horizontal axis. |
| `Invert X (+/-)` | Reverses the sign of the X axis. |
| `Y Address` | OSC destination for the vertical axis. |
| `Invert Y (+/-)` | Reverses the sign of the Y axis. |
| `Deadband` | Sends an axis only after its raw value differs from the last sent value by at least this amount. The raw range is approximately `-127` to `127`; minimum is `1`. |
| `Out Min` | Output mapped from raw input `-127`. |
| `Out Max` | Output mapped from raw input `127`. |
| `Out Type` | `Float`, `Int`, or `String`. |

For example, Out Min `-1` and Out Max `1` produce approximately `0` at center and `-1`/`1` at the ends. X and Y use separate OSC addresses and are sent independently when their deadband is exceeded.

### Click

The green-accented section uses the same multi-message and Sequence controls as Key and Encoder Click.

## 9. Angle

| Parameter | Meaning |
| --- | --- |
| `Address` | OSC destination for the angle value. |
| `Resolution` | `8-bit` uses `0`–`255`; `12-bit` uses `0`–`4095`. |
| `Deadband` | Minimum raw-count change required before sending. Minimum is `1`. |
| `Out Min` | Output corresponding to sensor input `0`. |
| `Out Max` | Output corresponding to `255` in 8-bit mode or `4095` in 12-bit mode. |
| `Out Type` | `Float`, `Int`, or `String`. |

With 12-bit resolution and an output range of `0` to `1`, a midpoint reading sends approximately `0.5`. The first reading initializes the state; later values are sent after the deadband is exceeded.

## 10. ToF

Chain ToF converts measured distance into an OSC value.

| Parameter | Meaning |
| --- | --- |
| `Address` | OSC destination for the mapped value; this is not necessarily the raw distance in millimeters. |
| `Deadband (mm)` | Minimum change from the last valid distance before sending. Range: `1`–`2000` mm. |
| `Maximum Distance (mm)` | Exclusive upper limit of the active measurement range. Range: `31`–`2000` mm. |
| `Output Direction` | Select `Near → Out Min / Far → Out Max` or `Near → Out Max / Far → Out Min`. |
| `Out Min` | One end of the output range. |
| `Out Max` | The other end of the output range. |
| `Out Type` | `Float` or rounded `Int`. |

> [!NOTE]
> The active range is 30 mm or greater and below Maximum Distance. When the target leaves this range, OSC transmission stops instead of sending a maximum-distance value. Transmission resumes with the first valid reading.

For example, with Maximum Distance `500`, output `0`–`1`, and `Near → Out Max / Far → Out Min`, a reading near 30 mm sends approximately `1` and approaches `0` near 500 mm. At 500 mm or beyond, transmission stops.

## 11. Backup and restore

Export JSON before major changes, firmware updates, or erasing all settings. The backup includes:

- Format and schema version
- OSC destination
- Display rotation
- Saved device UIDs, names, and device-specific settings
- Multi-message settings for Key, Encoder Click, and Joystick Click

Wi-Fi SSID and password are not included. The current JSON file limit is 48 KiB.

## 12. Erase all settings

Press and hold the AtomS3R screen for ten seconds. Progress appears on the display. This permanently erases:

- Wi-Fi settings
- OSC destination
- Saved device settings
- Display rotation
- Known-device list

Export any required settings first.

## 13. Troubleshooting

### The settings page does not open

- Confirm that the AtomS3R and browser device are on the same Wi-Fi network.
- If `.local` does not work, use the IP address shown on the AtomS3R.
- Restart the AtomS3R and check whether its IP address changed.

### A Chain device does not appear

- Check the Atomic ToChain Base and device connections.
- Disconnect the device, wait a few seconds, and reconnect it.
- Settings for a temporary placeholder UID cannot be stored permanently.

### OSC is not received

- Check Host IP and Port.
- Confirm that the AtomS3R and receiver are on the same network.
- Check the computer firewall and the receiving application's OSC settings.
- Confirm that the OSC Address starts with `/`.

### JSON cannot be imported

- Use JSON exported by M5ChainOSC.
- The file must contain the supported `format` and `schemaVersion`.
- Keep the file at or below 48 KiB.
- Invalid addresses or values, duplicate UIDs, and more than eight click messages are rejected.

### Settings revert after saving

- Confirm that `Save All Settings` reported success.
- Check that the device UID was obtained correctly.
- If the problem continues, report the branch or commit, reproduction steps, and serial logs.
