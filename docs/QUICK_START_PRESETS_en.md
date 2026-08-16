---
layout: default
title: M5ChainOSC Preset Quick Start
permalink: /en/quick-start-presets/
---

# M5ChainOSC Preset Quick Start

[日本語版](../../quick-start-presets/)

This guide takes you from firmware installation to sending OSC to VRChat with a ready-made device preset. See the [English User Guide](../user-guide/) for complete settings documentation.

> [!IMPORTANT]
> M5ChainOSC is an unofficial, independently developed project. It is not an official product of M5Stack Technology Co., Ltd. or VRChat Inc.

## What you need

- M5Stack AtomS3R
- M5Stack Atomic ToChain Base
- A supported M5Stack Chain device
- A USB cable that supports data
- A 2.4 GHz Wi-Fi network
- A computer running VRChat
- Desktop Google Chrome or Microsoft Edge

## 1. Install the firmware

1. Open the [M5ChainOSC Web Installer](https://shimez.github.io/M5ChainOSC/installer/) in Chrome or Edge.
2. Connect the AtomS3R to the computer by USB.
3. Select `Install M5ChainOSC` and choose the AtomS3R serial port.
4. Follow the on-screen instructions.

If the port is missing, hold the physical button on the AtomS3R while reconnecting USB.

## 2. Configure Wi-Fi

1. Start the AtomS3R.
2. Connect a phone or computer to `AtomS3R-OSC`.
3. Enter the password `12345678`.
4. If the portal does not open, visit `http://192.168.4.1/`.
5. Save the SSID and password of the network the AtomS3R should use.

The AtomS3R supports 2.4 GHz Wi-Fi only. Connect it and the VRChat computer to the same network where they can communicate with each other.

## 3. Enable OSC in VRChat

```text
Action Menu → Options → OSC → Enabled
```

## 4. Find the computer's IP address

On Windows, run this in PowerShell or Command Prompt:

```powershell
ipconfig
```

Find the `IPv4 Address` of the Wi-Fi or Ethernet adapter connected to the same network as the AtomS3R. Do not use a VPN or virtual adapter address.

## 5. Open M5ChainOSC settings

1. Connect the Chain device you want to use.
2. Open the IP address shown on the AtomS3R, or `http://atoms3r-osc.local/`.
3. Enter the computer's IPv4 address in `OSC Destination` → `Host IP`.
4. Set `Port` to VRChat's standard receiving port, `9000`.

## 6. Download a preset

Open [Device Presets](https://github.com/shimez/M5ChainOSC/tree/main/presets), choose a JSON file for the same device type, and select GitHub's `Download raw file` button.

| Device | Preset | Behavior |
| --- | --- | --- |
| Joystick | `joystick/vrchat-move.json` | Move with the stick and jump by pressing it. |
| Key | `key/vrchat-voice-control.json` | Toggle microphone state. |
| Key | `key/vrchat-camera-controls.json` | Change several streaming camera settings. |
| Key | `key/vrchat-afk-control.json` | Toggle AFK mode. |
| Angle | `angle/vrchat-camerazoom.json` | Adjust camera zoom by angle. |
| Encoder | `encoder/vrchat-camerazoom.json` | Adjust camera zoom by rotation. |

## 7. Import the preset

1. Find the target device in the Web UI.
2. Open `…` in the upper-right corner of its card.
3. Select `Import Preset (JSON)`.
4. Choose the downloaded JSON file.
5. Confirm the import.
6. Select `Save All Settings`.

A preset can only be imported into the same type of Chain device. Presets do not contain a UID or Device Name, so the connected device keeps its identity.

## 8. Test it

With VRChat running and OSC enabled, operate the Chain device:

- Move the Joystick and confirm that the avatar moves; press it to jump.
- Press a Key and confirm the microphone or selected state changes.
- Open the VRChat camera and operate Angle or Encoder to confirm zoom changes.

## If OSC is not received

- Confirm OSC is enabled in VRChat.
- Confirm Host IP is the current IPv4 address of the VRChat computer.
- Confirm Port is `9000`.
- Confirm the AtomS3R and computer can communicate on the same network.
- Check Windows Defender Firewall or other firewall software.
- Press `Save All Settings` after editing.
- Confirm the preset matches the connected device type.

See the [English User Guide](../user-guide/) for more troubleshooting and parameter details.
