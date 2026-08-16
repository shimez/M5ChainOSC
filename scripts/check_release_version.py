#!/usr/bin/env python3
"""Validate version references before building a release."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


def read_text(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require_match(pattern: str, text: str, description: str) -> str:
    match = re.search(pattern, text)
    if not match:
        raise SystemExit(f"Could not find {description}.")
    return match.group(1)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--tag",
        default="",
        help="Release tag such as v1.6.0. Leave empty for a manual dry run.",
    )
    parser.add_argument("--github-output", default="")
    parser.add_argument(
        "--check-installer",
        action="store_true",
        help="Validate Web Installer metadata and the local firmware file.",
    )
    parser.add_argument(
        "--check-installer-metadata",
        action="store_true",
        help="Validate Web Installer version references without requiring firmware.",
    )
    args = parser.parse_args()

    config_version = require_match(
        r'APP_VERSION\s*=\s*"([^"]+)"',
        read_text("src/config.h"),
        "APP_VERSION in src/config.h",
    )
    if not SEMVER_RE.fullmatch(config_version):
        raise SystemExit(f"APP_VERSION is not valid SemVer: {config_version}")

    if args.tag:
        if not re.fullmatch(r"v(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)", args.tag):
            raise SystemExit(f"Release tag must use vX.Y.Z format: {args.tag}")
        tag_version = args.tag[1:]
        if tag_version != config_version:
            raise SystemExit(
                f"Tag version {tag_version} does not match APP_VERSION {config_version}."
            )

    firmware_name = f"M5ChainOSC-{config_version}-AtomS3R-merged.bin"
    checksum_name = f"M5ChainOSC-{config_version}-AtomS3R-SHA256.txt"

    if args.check_installer or args.check_installer_metadata:
        manifest = json.loads(read_text("docs/installer/manifest.json"))
        manifest_version = str(manifest.get("version", ""))
        if manifest_version != config_version:
            raise SystemExit(
                f"manifest.json version {manifest_version} does not match "
                f"APP_VERSION {config_version}."
            )

        expected_manifest_path = f"firmware/{firmware_name}"
        try:
            part = manifest["builds"][0]["parts"][0]
        except (KeyError, IndexError, TypeError) as exc:
            raise SystemExit(
                "manifest.json does not contain the expected firmware part."
            ) from exc

        if part.get("path") != expected_manifest_path or part.get("offset") != 0:
            raise SystemExit(
                "manifest.json must reference "
                f"{expected_manifest_path} at offset 0."
            )

        if args.check_installer and not (
            ROOT / "docs" / "installer" / expected_manifest_path
        ).is_file():
            raise SystemExit(
                "Web Installer firmware file does not exist: "
                f"docs/installer/{expected_manifest_path}"
            )

        installer_index = read_text("docs/installer/index.html")
        if f"Stable version {config_version}" not in installer_index:
            raise SystemExit("Installer index does not show the current stable version.")

        installer_readme = read_text("docs/installer/README.md")
        if f"現在の正式版は`{config_version}`です。" not in installer_readme:
            raise SystemExit("Installer README does not show the current stable version.")

    print(f"Validated release version {config_version}")
    print(f"Firmware: {firmware_name}")

    if args.github_output:
        output_path = Path(args.github_output)
        with output_path.open("a", encoding="utf-8") as output:
            output.write(f"version={config_version}\n")
            output.write(f"firmware_name={firmware_name}\n")
            output.write(f"checksum_name={checksum_name}\n")


if __name__ == "__main__":
    main()
