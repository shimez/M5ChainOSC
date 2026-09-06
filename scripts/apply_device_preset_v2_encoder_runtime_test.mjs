#!/usr/bin/env node

import process from "node:process";

const PROFILES = new Set([
  "amount-wrap",
  "amount-stop",
  "amount-reverse",
  "direction",
  "legacy",
]);

function usage() {
  console.log(`Usage:
  node scripts/apply_device_preset_v2_encoder_runtime_test.mjs \\
    --base-url <URL> \\
    --encoder-index <index> \\
    --profile <amount-wrap|amount-stop|amount-reverse|direction|legacy> \\
    --confirm-device-settings-overwrite

This helper overwrites only the selected connected Encoder settings. It does
not restore the previous settings and does not judge physical runtime output.`);
}

function option(name) {
  const position = process.argv.indexOf(name);
  return position >= 0 ? process.argv[position + 1] : undefined;
}

if (process.argv.includes("--help") || process.argv.length === 2) {
  usage();
  process.exit(process.argv.includes("--help") ? 0 : 2);
}

const baseUrlText = option("--base-url");
const encoderIndexText = option("--encoder-index");
const profile = option("--profile");
let baseUrl;
try {
  baseUrl = new URL(baseUrlText);
} catch {
  console.error("Invalid or missing --base-url.");
  usage();
  process.exit(2);
}
if (!/^https?:$/.test(baseUrl.protocol)) {
  console.error("--base-url must use http or https.");
  process.exit(2);
}
baseUrl.pathname = baseUrl.pathname.replace(/\/$/, "");
const encoderIndex = Number(encoderIndexText);
if (!Number.isSafeInteger(encoderIndex) || encoderIndex < 0) {
  console.error("--encoder-index must be a non-negative integer.");
  process.exit(2);
}
if (!PROFILES.has(profile)) {
  console.error("Invalid or missing --profile.");
  usage();
  process.exit(2);
}

console.log(`Target:
  ${baseUrl.href.replace(/\/$/, "")}

Encoder index:
  ${encoderIndex}

Profile:
  ${profile}

WARNING:
  Selected Encoder settings will be overwritten.`);

if (!process.argv.includes("--confirm-device-settings-overwrite")) {
  console.error("\nNo settings were changed. Add --confirm-device-settings-overwrite to continue.");
  process.exit(2);
}

const addressRoot = `/chainosc/test/encoder/${encoderIndex}`;
const push = {
  pushMode: 0,
  press: [],
  release: [],
  sequence: {
    address: `${addressRoot}/push/sequence`,
    type: 1,
    start: 0,
    end: 1,
    step: 1,
  },
};

function v2Amount({ wrap, clockwiseIncreases }) {
  return {
    format: "ChainOSC-device-preset",
    schemaVersion: 2,
    deviceType: 1,
    deviceTypeName: "Encoder",
    encoder: {
      rotationAddress: `${addressRoot}/amount`,
      rotationMode: "amount",
      rangeSteps: 4,
      wrap,
      clockwiseIncreases,
      outputMin: 0,
      outputMax: 1,
      outputType: 0,
      ...push,
    },
  };
}

const presets = {
  "amount-wrap": v2Amount({ wrap: true, clockwiseIncreases: true }),
  "amount-stop": v2Amount({ wrap: false, clockwiseIncreases: true }),
  "amount-reverse": v2Amount({ wrap: false, clockwiseIncreases: false }),
  direction: {
    format: "ChainOSC-device-preset",
    schemaVersion: 2,
    deviceType: 1,
    deviceTypeName: "Encoder",
    encoder: {
      rotationAddress: `${addressRoot}/direction`,
      rotationMode: "direction",
      clockwiseValue: 100,
      counterClockwiseValue: -100,
      outputType: 1,
      ...push,
    },
  },
  // ChainOSC aa22fc356b621048c6576b0acf3a8f62d9e8a8ea:
  // migration/v1-amount-zero-based-input.json (legacy-import).
  legacy: {
    format: "ChainOSC-device-preset",
    schemaVersion: 1,
    deviceType: 1,
    deviceTypeName: "Encoder",
    encoder: {
      rotationAddress: `${addressRoot}/legacy`,
      sendIncrement: false,
      wrapAround: true,
      absoluteInputMin: 0,
      absoluteInputMax: 20,
      incrementScale: 0.05,
      range: { outMin: 0, outMax: 1, type: 0 },
      clickMode: 0,
      press: [],
      release: [],
      sequence: {
        address: `${addressRoot}/push/sequence`,
        type: 1,
        start: 0,
        end: 3,
        step: 1,
      },
    },
  },
};

const selected = presets[profile];
const importUrl = new URL("/import_device_preset", baseUrl);
importUrl.searchParams.set("index", String(encoderIndex));
importUrl.searchParams.set("ajax", "1");
const imported = await fetch(importUrl, {
  method: "POST",
  headers: { "content-type": "application/json" },
  body: JSON.stringify(selected),
});
const importMessage = await imported.text();
if (!imported.ok) {
  console.error(`\nImport failed: HTTP ${imported.status}\n${importMessage}`);
  process.exit(1);
}

const exportUrl = new URL("/export_device_preset", baseUrl);
exportUrl.searchParams.set("index", String(encoderIndex));
const exportedResponse = await fetch(exportUrl);
const exportedText = await exportedResponse.text();
if (!exportedResponse.ok) {
  console.error(`\nImport succeeded, but sanity-check export failed: HTTP ${exportedResponse.status}\n${exportedText}`);
  process.exit(1);
}

let exported;
try {
  exported = JSON.parse(exportedText);
} catch {
  console.error("\nImport succeeded, but the exported preset was not valid JSON.");
  process.exit(1);
}
const expectedSchemaVersion = profile === "legacy" ? 1 : 2;
const encoder = exported.encoder;
let modelShapeValid = false;
if (profile === "legacy") {
  modelShapeValid = Object.hasOwn(encoder ?? {}, "clickMode") &&
    !Object.hasOwn(encoder ?? {}, "pushMode") &&
    !Object.hasOwn(encoder ?? {}, "rotationMode");
} else if (selected.encoder.rotationMode === "amount") {
  modelShapeValid = ["rangeSteps", "wrap", "clockwiseIncreases", "outputMin",
    "outputMax", "pushMode", "press", "release", "sequence"]
    .every((field) => Object.hasOwn(encoder ?? {}, field)) &&
    !Object.hasOwn(encoder ?? {}, "clockwiseValue") &&
    !Object.hasOwn(encoder ?? {}, "counterClockwiseValue") &&
    !Object.hasOwn(encoder ?? {}, "clickMode");
} else {
  modelShapeValid = ["clockwiseValue", "counterClockwiseValue", "pushMode",
    "press", "release", "sequence"]
    .every((field) => Object.hasOwn(encoder ?? {}, field)) &&
    !["rangeSteps", "wrap", "clockwiseIncreases", "outputMin", "outputMax",
      "clickMode"].some((field) => Object.hasOwn(encoder ?? {}, field)) &&
    typeof encoder.clockwiseValue === "number" &&
    typeof encoder.counterClockwiseValue === "number";
}
if (exported.schemaVersion !== expectedSchemaVersion ||
    exported.deviceType !== 1 || !modelShapeValid ||
    encoder?.rotationAddress !== selected.encoder.rotationAddress ||
    (profile !== "legacy" && encoder?.rotationMode !== selected.encoder.rotationMode)) {
  console.error("\nImport succeeded, but the exported model did not pass the profile sanity check.");
  console.error(JSON.stringify(exported, null, 2));
  process.exit(1);
}

const guidance = {
  "amount-wrap": `Expected CW sequence:
  0.25 -> 0.50 -> 0.75 -> 1.00 -> 0.00 -> 0.25

Check:
- Import itself emitted no OSC
- First step starts from logicalPosition 0
- Maximum endpoint 1.00 is emitted
- The next step wraps to 0.00`,
  "amount-stop": `Expected CW sequence:
  0.25 -> 0.50 -> 0.75 -> 1.00 -> 1.00 -> 1.00

Check:
- There is no hidden overshoot
- The endpoint is resent for every outward step`,
  "amount-reverse": `Expected CCW sequence:
  0.25 -> 0.50 -> 0.75 -> 1.00

Check:
- CCW increases the logical position
- CW decreases the logical position`,
  direction: `Expected:
  CW  -> 100
  CCW -> -100

Check:
- One OSC message is sent for each non-zero delta sign
- A larger delta magnitude does not multiply the message count`,
  legacy: `Source fixture:
  migration/v1-amount-zero-based-input.json
  Expected migration result: legacy-import

Check:
- Export remains schemaVersion 1 with clickMode
- Legacy Wrap runtime semantics are preserved
- Applying the Legacy profile does not trigger a V2 runtime reset`,
};

console.log(`\nApplied runtime test profile: ${profile}`);
console.log(`Exporter sanity check: PASS (schemaVersion ${exported.schemaVersion})`);
console.log(`OSC Address: ${selected.encoder.rotationAddress}\n`);
console.log(guidance[profile]);
console.log(`
Two-Encoder UID continuity guide

Test A - Different UID reset:
1. Apply an Amount profile to Encoder A and advance its position.
2. Disconnect A and connect Encoder B.
3. Confirm B starts from logicalPosition 0.

Test B - Same UID reconnect:
1. Advance Encoder A to an intermediate position.
2. Disconnect and reconnect the same A.
3. Confirm the first recovered IncValue is discarded with no OSC.
4. Confirm the next step continues from A's previous logicalPosition.

Test C - A -> B -> A:
1. Advance A to position 3, then replace it with B.
2. Confirm B starts at 0 and advance B to position 2.
3. Reconnect A and confirm it resumes at position 3.
4. Reconnect B and, if its cache entry remains, confirm it resumes at 2.

Two Encoders connected simultaneously:
- Run this helper separately with --encoder-index 0 and --encoder-index 1.
- Use the index-specific OSC Addresses to distinguish their output.
- Confirm rotating one Encoder never changes the other's logicalPosition.`);
