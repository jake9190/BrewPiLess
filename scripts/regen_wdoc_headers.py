#!/usr/bin/env python3
"""Regenerate wdoc/*.h embedded web page headers from htmljs/dist.

The firmware embeds pre-built, gzip-compressed web pages as C byte arrays in
wdoc/*.h. Those files must be regenerated whenever htmljs/dist changes,
otherwise the ESP32/ESP8266 keeps serving an old bundle (e.g. an old
JSVERSION) even though the frontend source/dist looks up to date.

This script mirrors htmljs/output.sh but:
- runs on Windows without bash/xxd
- falls back to the plain page when a "_s" (single-page) or "_e32" variant
  does not exist for a given language
"""
from __future__ import annotations

import gzip
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIST_DIR = ROOT / "htmljs" / "dist"
WDOC_DIR = ROOT / "wdoc"

LANGUAGES = [
    "norwegian",
    "english",
    "spanish",
    "portuguese-br",
    "slovak",
    "chinese",
    "italian",
]

# (output file suffix, C variable name, preferred source filenames in priority order)
PAGES = [
    ("index_htm", "data_index_htm_gz", ["index_s.htm", "index.htm"]),
    ("control_htm", "control_htm_gz", ["control_s.htm", "control.htm"]),
    ("config_htm", "config_htm_gz", ["config.htm"]),
    ("setup_htm", "setup_htm_gz", ["setup.htm"]),
    ("log_htm", "logging_htm_gz", ["logging.htm"]),
    ("gdc_htm", "gravity_htm_gz", ["gravity.htm"]),
    ("gdc_e32_htm", "gravity_htm_gz", ["gravity_e32.htm", "gravity.htm"]),
    ("pressure_htm", "pressure_htm_gz", ["pressure.htm"]),
]


def find_source(lang_dir: Path, candidates: list[str]) -> Path:
    for name in candidates:
        candidate = lang_dir / name
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"None of {candidates} found in {lang_dir}")


def write_header(variable: str, gz_bytes: bytes, out_path: Path) -> None:
    lines = [f"const unsigned char {variable}[] PROGMEM = {{"]
    for i in range(0, len(gz_bytes), 12):
        chunk = gz_bytes[i : i + 12]
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.append("};")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    for lang in LANGUAGES:
        lang_dir = DIST_DIR / lang
        if not lang_dir.exists():
            print(f"skip {lang}: no dist folder")
            continue

        for suffix, variable, candidates in PAGES:
            try:
                src = find_source(lang_dir, candidates)
            except FileNotFoundError as e:
                print(f"skip {lang}_{suffix}: {e}")
                continue

            html_bytes = src.read_bytes()
            gz_bytes = gzip.compress(html_bytes, compresslevel=9, mtime=0)

            out_path = WDOC_DIR / f"{lang}_{suffix}.h"
            write_header(variable, gz_bytes, out_path)
            print(f"wrote {out_path} from {src.relative_to(ROOT)} ({len(html_bytes)} -> {len(gz_bytes)} bytes)")


if __name__ == "__main__":
    main()
