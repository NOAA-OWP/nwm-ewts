from __future__ import annotations

from pathlib import Path

from .formatter import compact_utc_timestamp

def default_run_logs_dir() -> Path:
    return Path.home() / "run_logs"

def make_log_path(
    log_dir: Path,
    ewts_id: str,
    mpi_rank: int = -1,
    log_file_name: str | None = None,
) -> Path:

    if log_file_name:
        return log_dir / log_file_name

    ts = compact_utc_timestamp()
    safe_id = (ewts_id or "UNKNOWN").upper()

    rank_part = f"_rank_{mpi_rank}" if mpi_rank >= 0 else ""

    return log_dir / f"{safe_id}{rank_part}_{ts}.log"
