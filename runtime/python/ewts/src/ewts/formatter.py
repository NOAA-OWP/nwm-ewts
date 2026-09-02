from __future__ import annotations

from datetime import datetime, timezone

from .log_levels import LEVELS, log_level_name

EWTS_ID_WIDTH = 8
LEVEL_WIDTH = 7

def iso_utc_timestamp_ms() -> str:
    # Match C++: YYYY-MM-DDTHH:MM:SS.mmmZ
    now = datetime.now(timezone.utc)
    ms = int(now.microsecond / 1000)
    return now.strftime("%Y-%m-%dT%H:%M:%S.") + f"{ms:03d}Z"

def compact_utc_timestamp() -> str:
    # Match C++: YYYYMMDDTHHMMSS
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S")

def pad_ewts_id(ewts_id: str) -> str:
    s = (ewts_id or "").upper()
    if len(s) >= EWTS_ID_WIDTH:
        return s[:EWTS_ID_WIDTH]
    return s + (" " * (EWTS_ID_WIDTH - len(s)))

def fixed_level_name(level: int) -> str:
    # Prefer canonical name for the exact value if present.
    name = log_level_name(int(level))
    # Normalize common display expectations:
    if name == "ERROR":
        # C++ prints SEVERE for 40 by default; generated constants may include ERROR=40 too.
        # Keep ERROR only if you explicitly want it; default here matches C++ style.
        name = "SEVERE"
    out = name
    if len(out) < LEVEL_WIDTH:
        out = out + (" " * (LEVEL_WIDTH - len(out)))
    if len(out) > LEVEL_WIDTH:
        out = out[:LEVEL_WIDTH]
    return out

def format_prefix(ewts_id: str, level: int) -> str:
    # Match C++: "<ISO timestamp> <EWTS_ID padded> <LEVEL padded>"
    return f"{iso_utc_timestamp_ms()} {pad_ewts_id(ewts_id)} {fixed_level_name(level)}"

def split_lines(message: str) -> list[str]:
    # Match C++ behavior: stream getline over message.
    # If message ends with newline, Python splitlines() would drop the last empty line by default.
    # We want behavior close to getline: it emits an empty final line only if there is a trailing delimiter?
    # Simpler: use splitlines() without keeping ends; if message is empty, emit one empty line.
    if message is None:
        return [""]
    lines = str(message).splitlines()
    return lines if lines else [""]
