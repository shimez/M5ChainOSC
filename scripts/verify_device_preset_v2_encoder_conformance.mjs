#!/usr/bin/env node

import fs from "node:fs/promises";
import path from "node:path";
import process from "node:process";

function usage() {
  console.log(`Usage:
  node scripts/verify_device_preset_v2_encoder_conformance.mjs \\
    --base-url http://m5chainosc.local \\
    --encoder-index 0 \\
    --chainosc C:\\path\\to\\ChainOSC \\
    --confirm-device-settings-overwrite

This runner sends the common Encoder fixtures to the real M5ChainOSC HTTP
Importer and reads them back through the real Exporter. The selected Encoder's
settings are overwritten during the test. It does not test physical rotation,
OSC reception, reconnect behavior, or reboot persistence.`);
}

function option(name) {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : undefined;
}

if (process.argv.includes("--help") || process.argv.length === 2) {
  usage();
  process.exit(process.argv.includes("--help") ? 0 : 2);
}

const baseUrl = option("--base-url")?.replace(/\/$/, "");
const encoderIndex = option("--encoder-index");
const chainOscRoot = option("--chainosc");
if (!baseUrl || encoderIndex === undefined || !chainOscRoot ||
    !process.argv.includes("--confirm-device-settings-overwrite")) {
  usage();
  process.exit(2);
}

const fixtureRoot = path.join(chainOscRoot, "test-data", "device-presets-v2");
const results = [];

async function readJson(file) {
  return JSON.parse(await fs.readFile(file, "utf8"));
}

async function request(relativeUrl, init = {}) {
  const response = await fetch(`${baseUrl}${relativeUrl}`, init);
  return { status: response.status, text: await response.text() };
}

async function importPreset(preset) {
  return request(`/import_device_preset?index=${encodeURIComponent(encoderIndex)}&ajax=1`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(preset),
  });
}

async function exportPreset() {
  const response = await request(
    `/export_device_preset?index=${encodeURIComponent(encoderIndex)}`,
  );
  if (response.status !== 200) {
    throw new Error(`export failed: HTTP ${response.status}: ${response.text}`);
  }
  return JSON.parse(response.text);
}

function equivalent(expected, actual, location = "$") {
  if (typeof expected === "number" && typeof actual === "number") {
    return Object.is(Math.fround(expected), Math.fround(actual))
      ? null : `${location}: expected ${expected}, actual ${actual}`;
  }
  if (typeof expected !== typeof actual || expected === null || actual === null) {
    return Object.is(expected, actual)
      ? null : `${location}: expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`;
  }
  if (Array.isArray(expected)) {
    if (!Array.isArray(actual) || expected.length !== actual.length)
      return `${location}: array length/type differs`;
    for (let i = 0; i < expected.length; i++) {
      const difference = equivalent(expected[i], actual[i], `${location}[${i}]`);
      if (difference) return difference;
    }
    return null;
  }
  if (typeof expected === "object") {
    const expectedKeys = Object.keys(expected).sort();
    const actualKeys = Object.keys(actual).sort();
    if (expectedKeys.join("\0") !== actualKeys.join("\0"))
      return `${location}: properties differ (${expectedKeys} vs ${actualKeys})`;
    for (const key of expectedKeys) {
      const difference = equivalent(expected[key], actual[key], `${location}.${key}`);
      if (difference) return difference;
    }
    return null;
  }
  return Object.is(expected, actual)
    ? null : `${location}: expected ${JSON.stringify(expected)}, actual ${JSON.stringify(actual)}`;
}

function record(category, id, passed, detail = "") {
  results.push({ category, id, passed, detail });
  console.log(`${passed ? "PASS" : "FAIL"} ${category}: ${id}${detail ? ` (${detail})` : ""}`);
}

async function fixtureFiles(directory) {
  const entries = await fs.readdir(path.join(fixtureRoot, directory));
  return entries.filter((name) => name.endsWith(".json")).sort();
}

for (const directory of ["canonical", "valid"]) {
  for (const name of await fixtureFiles(directory)) {
    const preset = await readJson(path.join(fixtureRoot, directory, name));
    if (preset.deviceType !== 1) continue;
    const id = `${directory}/${name}`;
    const imported = await importPreset(preset);
    if (imported.status !== 200) {
      record("valid-import", id, false, `HTTP ${imported.status}: ${imported.text}`);
      continue;
    }
    const exported = await exportPreset();
    const difference = equivalent(preset, exported);
    record("valid-import", id, !difference, difference ?? "V2/D3 observable as schemaVersion 2");
    if (difference) continue;
    const reimported = await importPreset(exported);
    const reexported = reimported.status === 200 ? await exportPreset() : null;
    const roundTripDifference = reexported ? equivalent(exported, reexported) :
      `re-import HTTP ${reimported.status}: ${reimported.text}`;
    record(`round-trip-${preset.encoder.rotationMode}`, id,
           !roundTripDifference, roundTripDifference ?? "equivalent V2 model");
  }
}

const expectedErrors = await readJson(path.join(fixtureRoot, "expected-errors.json"));
for (const [relative, expectedCode] of Object.entries(expectedErrors.fixtures)) {
  const preset = await readJson(path.join(fixtureRoot, relative));
  if (preset.deviceType !== 1) continue;
  const before = await exportPreset();
  const imported = await importPreset(preset);
  const after = await exportPreset();
  const code = imported.text.match(/E_[A-Z0-9_]+/)?.[0];
  const unchanged = !equivalent(before, after);
  record("invalid-import", relative,
         imported.status >= 400 && code === expectedCode && unchanged,
         `HTTP ${imported.status}, code=${code ?? "none"}, unchanged=${unchanged}`);
}

const migrationRoot = path.join(fixtureRoot, "migration");
const migrationManifest = await readJson(path.join(migrationRoot, "cases.json"));
for (const testCase of migrationManifest.cases) {
  const input = await readJson(path.join(migrationRoot, testCase.input));
  const before = await exportPreset();
  const imported = await importPreset(input);
  if (testCase.outcome === "import-error") {
    const after = await exportPreset();
    const code = imported.text.match(/E_[A-Z0-9_]+/)?.[0];
    const unchanged = !equivalent(before, after);
    record("migration", testCase.id,
           imported.status >= 400 && code === testCase.expectedError && unchanged,
           `import-error code=${code ?? "none"}, unchanged=${unchanged}`);
    continue;
  }
  if (imported.status !== 200) {
    record("migration", testCase.id, false,
           `HTTP ${imported.status}: ${imported.text}`);
    continue;
  }
  const exported = await exportPreset();
  const expectedFile = testCase.outcome === "v2-migration"
    ? testCase.expectedV2 : testCase.expectedLegacy;
  const expected = await readJson(path.join(migrationRoot, expectedFile));
  const difference = equivalent(expected, exported);
  record("migration", testCase.id, !difference,
         difference ?? `${testCase.outcome}, schemaVersion=${exported.schemaVersion}`);
}

const failed = results.filter((result) => !result.passed);
const categories = new Map();
for (const result of results) {
  const current = categories.get(result.category) ?? { passed: 0, total: 0 };
  current.total++;
  if (result.passed) current.passed++;
  categories.set(result.category, current);
}
console.log("\nSummary");
for (const [category, count] of categories)
  console.log(`${category}: ${count.passed}/${count.total} PASS`);
console.log("N/A common fixtures: Key fixtures (Phase 4C scope is Encoder)");
process.exit(failed.length ? 1 : 0);
