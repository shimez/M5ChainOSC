---
layout: default
title: M5ChainOSC Quick Start
permalink: /en/quick-start/
---

# M5ChainOSC Quick Start

[日本語版](../../quick-start/)

This guide configures one M5Stack Chain Key manually and verifies OSC messages from M5ChainOSC in VRChat. See the [English User Guide](../user-guide/) for details.

## What you need

- M5Stack AtomS3R
- M5Stack Atomic ToChain Base
- M5Stack Chain Key
- A data-capable USB cable
- A 2.4 GHz Wi-Fi network
- A computer running VRChat
- Desktop Google Chrome or Microsoft Edge

## 1. Install the firmware

1. Open the [M5ChainOSC Web Installer](https://shimez.github.io/M5ChainOSC/installer/) in Chrome or Edge.
2. Connect the AtomS3R to the computer by USB.
3. Select `Install M5ChainOSC` and choose the AtomS3R serial port.
4. Follow the on-screen instructions.

If the port is missing, hold the AtomS3R button while reconnecting USB. This is a firmware installation operation, not an OSC input.

## 2. Configure Wi-Fi

1. Start the AtomS3R.
2. Connect to `AtomS3R-OSC`.
3. Enter password `12345678`.
4. If the settings page does not open, visit `http://192.168.4.1/`.
5. Save the credentials for a 2.4 GHz Wi-Fi network.

Connect the AtomS3R and the VRChat computer to the same network so they can communicate. The Web UI has no authentication; use it on a trusted local network.

## 3. Enable OSC in VRChat

Select **Action Menu → Options → OSC → Enabled** in VRChat.

## 4. Find the IPv4 address of the VRChat PC

In Windows PowerShell or Command Prompt, run `ipconfig`. Find the IPv4 Address of the Wi-Fi or Ethernet adapter connected to the same network as the AtomS3R. Do not use a VPN or virtual network address.

## 5. Open the settings page and configure the destination

1. Connect the M5Stack Chain Key to the AtomS3R through the Atomic ToChain Base.
2. Open the IP address shown on the AtomS3R, or `http://m5chainosc.local/`.
3. Open the connected Chain Key card and enter the PC IPv4 address from step 4 in Host IP.
4. Enter `9000` in Port.

## 6. Configure a Voice action on the Chain Key

On the connected M5Stack Chain Key card, confirm that Key Mode is Press / Release. Under **Press**, enter:

- OSC Address: `/input/Voice`
- Type: `Int`
- Value: `1`

Switch to **Release** and enter:

- OSC Address: `/input/Voice`
- Type: `Int`
- Value: `0`

## 7. Save and verify the action

1. Select **Save All Settings**.
2. With VRChat running and OSC enabled, press the configured Chain Key.
3. Confirm that VRChat Voice becomes active.
4. Release the Chain Key and confirm that the Voice input state returns.

This confirms that M5ChainOSC sent an OSC message to VRChat. See the [English User Guide](../user-guide/) for Encoder, Joystick, Angle, ToF, Sequence, Device Presets, backups, and other details. Use Device Presets to reuse and share settings.
