from __future__ import annotations

from dataclasses import dataclass, replace
from pathlib import Path
from typing import Optional

from .helper import getenv_any
from .log_levels import LEVELS

_DEFAULT_LOG_DIR_NAME = "run_logs"

_RUNTIME_OVERRIDES: dict[str, "EwtsRuntimeOverride"] = {}

@dataclass
class EwtsRuntimeOverride:
    enabled: Optional[bool] = None
    default_level: Optional[int] = None
    log_dir: Optional[Path] = None
    log_file_name: Optional[str] = None
    running_in_ngen: Optional[bool] = None


def set_runtime_override(
    ewts_id: str,
    *,
    enabled: bool | None = None,
    default_level: int | None = None,
    log_dir: str | Path | None = None,
    log_file_name: str | None = None,
    running_in_ngen: bool | None = None,
) -> None:
    _RUNTIME_OVERRIDES[ewts_id.upper()] = EwtsRuntimeOverride(
        enabled=enabled,
        default_level=default_level,
        log_dir=Path(log_dir).expanduser() if log_dir is not None else None,
        log_file_name=log_file_name,
        running_in_ngen=running_in_ngen,
    )


def clear_runtime_override(ewts_id: str) -> None:
    _RUNTIME_OVERRIDES.pop(ewts_id.upper(), None)

def get_runtime_override(ewts_id: str) -> EwtsRuntimeOverride | None:
    return _RUNTIME_OVERRIDES.get(ewts_id.upper())

@dataclass(frozen=True)
class EwtsConfig:
    enabled: bool
    running_in_ngen: bool
    log_dir: Path | None
    default_level: int
    mpi_rank: int
    log_file_name: str | None = None 

def _env_bool(name: str, default: bool = True) -> bool:
    v = getenv_any(name, None)
    if v is None:
        return default
    return str(v).strip().lower() not in ("0", "false", "off", "no")

def is_ngen_active() -> bool:
    # ngen provides EWTS_USE_NGEN_BRIDGE when running within ngen.
    return bool(getenv_any("EWTS_USE_NGEN_BRIDGE", "").strip())

def get_log_dir() -> Path | None:
    v = getenv_any("EWTS_LOG_DIR", "").strip()
    if v:
        return Path(v).expanduser()
    return None

def _parse_level_value(v: str) -> int | None:
    s = (v or "").strip()
    if not s:
        return None
    # numeric
    if s.isdigit():
        try:
            return int(s)
        except Exception:
            return None
    key = s.upper()
    return LEVELS.get(key)

def get_default_level() -> int:
    v = getenv_any("EWTS_LOG_LEVEL", "").strip()
    parsed = _parse_level_value(v)
    # Default to INFO if unset/invalid
    return parsed if parsed is not None else LEVELS.get("INFO", 20)

def get_level_for_ewts_id(ewts_id: str) -> int:
    # Per-module override: <EWTSID>_LOGLEVEL, e.g. TROUTE_LOGLEVEL
    key = f"{ewts_id.upper()}_LOGLEVEL"
    v = getenv_any(key, "").strip()
    parsed = _parse_level_value(v)
    if parsed is not None:
        return parsed
    return get_default_level()

def get_mpi_rank() -> int:
    v = getenv_any("EWTS_RANK", "").strip()
    if not v:
        return -1
    try:
        return int(v)
    except (TypeError, ValueError):
        return -1

def load_config(ewts_id: str) -> EwtsConfig:
    ewts_id = ewts_id.upper()

    cfg = EwtsConfig(
        running_in_ngen=is_ngen_active(),
        enabled=_env_bool("EWTS_ENABLED", True),
        # Only used for standalone; safe to compute always.
        log_dir=get_log_dir(),
        default_level=get_level_for_ewts_id(ewts_id),
        mpi_rank=get_mpi_rank(),
    )

    ov = get_runtime_override(ewts_id)
    if ov is None:
        return cfg

    updates = {}
    if ov.running_in_ngen is not None:
        updates["running_in_ngen"] = ov.running_in_ngen
    if ov.enabled is not None:
        updates["enabled"] = ov.enabled
    if ov.log_dir is not None:
        updates["log_dir"] = ov.log_dir
    if ov.log_file_name is not None:
        updates["log_file_name"] = ov.log_file_name
    if ov.default_level is not None:
        updates["default_level"] = ov.default_level

    if updates:
        cfg = replace(cfg, **updates)

    return cfg
