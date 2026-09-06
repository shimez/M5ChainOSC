# Device Preset v2 Encoder Conformance Record --- M5ChainOSC

## Test Metadata

| Item | Value |
|---|---|
| Test date | 2026-09-07 |
| ChainOSC specification commit | `aa22fc356b621048c6576b0acf3a8f62d9e8a8ea` |
| ChainOSC branch | `main` |
| M5ChainOSC version | `1.12.0` |
| Tested M5ChainOSC implementation commit | `a71d6f806f65bd79bf988217f3ff81b7575e4d99` |
| M5ChainOSC branch | `feature/device-preset-v2-encoder` |
| Hardware | M5Stack AtomS3R with Chain Encoder |
| PlatformIO environment | `atoms3r` |

## 1. Purpose

This document records the conformance verification of the M5ChainOSC
Chain Encoder implementation against the ChainOSC Device Preset v2
Encoder specification.

This document is a test record, not a normative specification. The
normative definition is `DEVICE_PRESET_FORMAT_V2.md` at the ChainOSC
specification commit recorded above.

## 2. Scope

Implementation status: **Device Preset v2 Encoder implementation: COMPLETE**

The tested scope is Device Preset v2 for Chain Encoder Amount and
Direction modes, including Importer validation, export round trips,
V2 Push behavior, v1 migration classification, Legacy import and runtime,
explicit Legacy-to-V2 migration, Legacy/V2 Web UI, transactional Save,
Encoder runtime semantics, UID-based runtime continuity, and simultaneous
operation of two connected Chain Encoders.

Key, Angle, ToF, and Joystick Device Preset v2 behavior is outside this
record's scope. Their common fixtures are reported as N/A rather than
PASS or FAIL.

## 3. Verification Method

Verification used four distinct evidence layers:

1. The common ChainOSC Schema, fixtures, expected errors, migration
   cases, and runtime vectors were validated for internal consistency.
2. `scripts/verify_device_preset_v2_encoder_conformance.mjs` exercised
   the production M5ChainOSC HTTP Importer and Exporter. Invalid imports
   also verified that the settings were unchanged.
3. Phase 3 semantic tracing compared the M5ChainOSC Encoder runtime
   behavior with all 14 Encoder runtime vectors.
4. Physical hardware tests exercised imported settings through actual
   Chain Encoder input and observed OSC output.

`scripts/apply_device_preset_v2_encoder_runtime_test.mjs` only applies a
selected test profile and prints the manual procedure. It is not an OSC
receiver and does not independently judge runtime PASS/FAIL.

## 4. Common Fixture and Product Importer Results

| Verification item | Result |
|---|---|
| Common valid fixtures | 15 / 15 PASS |
| Applicable Encoder valid fixtures | 11 / 11 PASS |
| Key valid fixtures | 4 N/A |
| Common invalid fixtures | 23 / 23 PASS |
| Applicable Encoder invalid fixtures | 20 / 20 PASS |
| Key invalid fixtures | 3 N/A |
| Expected Error Registry codes for applicable invalid fixtures | 20 / 20 PASS |
| Invalid-import atomicity | PASS; settings remained unchanged |

For `invalid/encoder-amount-range-fractional.json`, fractional
`rangeSteps` was rejected with `E_PRESET_FIELD_TYPE_INVALID`. This is the
final expected result because `rangeSteps` requires a JSON Integer;
integer values outside `1..65535` instead use
`E_PRESET_DEVICE_SETTING_INVALID`.

## 5. Export and Round-Trip Results

| Rotation mode | Result |
|---|---|
| Amount | 7 / 7 PASS |
| Direction | 4 / 4 PASS |

D3/V2 settings were exported in the canonical Device Preset v2 Encoder
model. Re-import and re-export preserved the applicable semantic model.
Legacy settings were not implicitly promoted to D3/V2 and were not
implicitly exported as Device Preset v2.

## 6. v1 Import and Migration Results

| Outcome | Result |
|---|---|
| Total migration cases | 8 / 8 PASS |
| `v2-migration` | 1 / 1 PASS |
| `legacy-import` | 6 / 6 PASS |
| `import-error` | 1 / 1 PASS |

A valid v1 Encoder preset was promoted to D3/V2 only when migration
preserved runtime semantics. Valid v1 presets that could not be migrated
without changing semantics were accepted as Legacy settings and retained
Legacy-compatible storage/runtime classification. An invalid v1 preset
remained an import error.

This section records product-level Importer/migration classification and
persistence results. Legacy runtime behavior, Legacy normal Save, explicit
lossless migration, and the non-lossless manual editing path were also
verified during final RC hardware testing.

## 7. Encoder Runtime Vector Results

All 14 Encoder runtime vectors passed against the M5ChainOSC runtime:

| Runtime vector | Result |
|---|---|
| `AMOUNT-RESET-NO-SEND` | PASS |
| `AMOUNT-WRAP-INCLUDES-BOTH-ENDS` | PASS |
| `AMOUNT-CLAMP-HAS-NO-HIDDEN-OVERSHOOT` | PASS |
| `AMOUNT-COUNTER-CLOCKWISE-INCREASES` | PASS |
| `AMOUNT-STOP-ENDPOINT-RESENDS` | PASS |
| `AMOUNT-STRING-FIXED-THREE-DECIMALS` | PASS |
| `DIRECTION-USES-SIGN-ONCE` | PASS |
| `DIRECTION-STRING-VALUES-ARE-LITERAL` | PASS |
| `RECONNECT-BASELINE-PRESERVES-AMOUNT-POSITION` | PASS |
| `DIFFERENT-UID-RESETS-AMOUNT-POSITION` | PASS |
| `AMOUNT-INT-ROUNDS-HALF-AWAY-FROM-ZERO` | PASS |
| `AMOUNT-ACTIVE-SETTING-CHANGES-RESET` | PASS |
| `CHANGE-TO-AMOUNT-MODE-RESETS` | PASS |
| `AMOUNT-INACTIVE-DIRECTION-SETTING-CHANGES-DO-NOT-RESET` | PASS |

## 8. Physical Hardware End-to-End Results

The following behavior was confirmed with physical M5ChainOSC hardware
and Chain Encoders:

| Test | Observed result |
|---|---|
| Amount Wrap | Endpoint-inclusive sequence `0.25, 0.50, 0.75, 1.00, 0.00` |
| Amount Stop | Clamp, endpoint resend, and no hidden overshoot |
| Amount reverse direction | Counter-clockwise increased and clockwise decreased |
| Direction mode | Configured values and sign-once semantics |
| Same UID reconnect | Position preserved; recovery sample discarded without spurious OSC; next step continued |
| Different UID | New Encoder started from position zero |
| Encoder A to B to A | Per-UID cached positions were restored independently |
| Two connected Encoders | Simultaneous connection and independent operation passed |
| V2 Push Press/Release | Configured Press and Release messages were sent correctly |
| V2 Push Sequence | Sequence advanced and wrapped as configured |
| Legacy runtime | Existing Legacy behavior remained unchanged |
| Legacy normal Save | Settings remained Legacy after normal Save |
| Explicit lossless Legacy to V2 migration | Candidate generation and successful V2 transition passed |
| Non-lossless migration editing path | No automatic correction; manual V2 editing passed |
| Candidate before Save | No persistence, runtime change, reset, or OSC send |
| V2 Save success UI | Transitioned immediately to normal V2 UI without reload |
| Transactional Save failure | Legacy RAM, runtime, and persistence remained unchanged |
| Export before migration | Device Preset `schemaVersion` remained 1 |
| Export after migration | Device Preset `schemaVersion` changed to 2 only after successful Save |
| Reboot persistence | Saved V2 model and behavior were restored after reboot |
| Migration command URL | Migration query was removed after candidate generation |
| Refresh after candidate generation | Migration command was not replayed |
| RC hardware regression | PASS |

For the Amount profiles, importing the preset itself emitted no OSC and
the first subsequent Encoder step was applied from the reset position.

The simultaneous-connection result means two physical Chain Encoders
were connected and operated at the same time; it is distinct from the
A-to-B-to-A reconnect test.

## 9. Build Verification

The `atoms3r` PlatformIO environment was built successfully from the
tested source state.

| Resource | Usage | Result |
|---|---|---|
| RAM | 101,628 / 327,680 bytes (31.0%) | PASS |
| Flash | 1,239,285 / 3,342,336 bytes (37.1%) | PASS |

## 10. Final Result

| Verification layer | Evidence | Result |
|---|---|---|
| Common valid fixtures | Common fixture validation | 15 / 15 PASS |
| Common invalid fixtures | Common fixture validation | 23 / 23 PASS |
| Product Encoder valid imports | Production HTTP Importer | 11 / 11 PASS |
| Product Encoder invalid imports and codes | Production HTTP Importer | 20 / 20 PASS |
| Amount round trips | Production Importer/Exporter | 7 / 7 PASS |
| Direction round trips | Production Importer/Exporter | 4 / 4 PASS |
| v1 migration classification | Production Importer/Exporter | 8 / 8 PASS |
| Encoder runtime vectors | Runtime semantic trace | 14 / 14 PASS |
| Physical hardware runtime | M5ChainOSC and Chain Encoder | PASS |
| Two-Encoder simultaneous operation | Two connected Chain Encoders | PASS |
| PlatformIO `atoms3r` build | Build verification | PASS |
| Final Phase 5B RC hardware verification | Migration UI, Save, Export, reboot, and runtime checks | PASS |

Overall result for the tested M5ChainOSC Device Preset v2 Encoder scope:

```text
PASS
```

No known semantic mismatch remains in the tested scope.

## 11. Known Limitations and Reproducibility

- The two product scripts are product-level conformance tools, not an
  independent reimplementation of the normative validator.
- Runtime hardware observations were manually judged rather than
  captured by an automated OSC packet assertion system.
- Key and Sensor Device Preset v2 behavior is outside this record.

This record is reproducible using the exact ChainOSC and M5ChainOSC
commits, branches, version, hardware, and PlatformIO environment recorded
in Test Metadata. The tested implementation commit identifies the final
Phase 5B and RC implementation source before release-closure documentation
and version metadata updates.
