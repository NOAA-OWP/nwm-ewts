from pathlib import Path

import pytest

from ewts.config import (
    is_ngen_active,
    get_log_dir,
    get_default_level,
    get_level_for_ewts_id,
    load_config,
    set_runtime_override,
    clear_runtime_override,
)
from ewts.log_levels import LEVELS


def test_is_ngen_active_uses_bridge_env(clean_ewts_env, monkeypatch):
    assert is_ngen_active() is False

    monkeypatch.setenv("EWTS_USE_NGEN_BRIDGE", "1")
    assert is_ngen_active() is True


@pytest.mark.parametrize("value", ["", "   "])
def test_get_log_dir_unset_or_blank_defaults_to_stdout(clean_ewts_env, monkeypatch, value):
    monkeypatch.setenv("EWTS_LOG_DIR", value)
    assert get_log_dir() is None


def test_get_log_dir_env_override(clean_ewts_env, monkeypatch, tmp_path):
    monkeypatch.setenv("EWTS_LOG_DIR", str(tmp_path))
    assert get_log_dir() == tmp_path


@pytest.mark.parametrize(
    "val,expected",
    [
        ("", LEVELS.get("INFO", 20)),
        ("INFO", LEVELS["INFO"]),
        (" debug ", LEVELS["DEBUG"]),
        ("15", 15),
        ("SeVeRe", LEVELS["SEVERE"]),
        ("bogus", LEVELS.get("INFO", 20)),
    ],
)
def test_get_default_level_parsing(clean_ewts_env, monkeypatch, val, expected):
    if val == "":
        monkeypatch.delenv("EWTS_LOG_LEVEL", raising=False)
    else:
        monkeypatch.setenv("EWTS_LOG_LEVEL", val)
    assert get_default_level() == expected


def test_get_level_for_ewts_id_override(clean_ewts_env, monkeypatch):
    monkeypatch.setenv("EWTS_LOG_LEVEL", "DEBUG")
    monkeypatch.setenv("TROUTE_LOGLEVEL", "FATAL")
    assert get_level_for_ewts_id("TROUTE") == LEVELS["FATAL"]


def test_load_config_defaults_to_stdout_when_log_dir_unset(clean_ewts_env, monkeypatch):
    monkeypatch.delenv("EWTS_LOG_DIR", raising=False)
    monkeypatch.setenv("EWTS_LOG_LEVEL", "WARNING")

    cfg = load_config("TROUTE")

    assert cfg.running_in_ngen is False
    assert cfg.log_dir is None
    assert cfg.default_level == LEVELS["WARNING"]


def test_load_config_uses_log_dir_when_explicitly_set(clean_ewts_env, monkeypatch, tmp_path):
    monkeypatch.setenv("EWTS_LOG_DIR", str(tmp_path))

    cfg = load_config("TROUTE")

    assert cfg.log_dir == tmp_path


def test_load_config_applies_runtime_override(clean_ewts_env, tmp_path):
    set_runtime_override(
        "TROUTE",
        enabled=False,
        default_level=LEVELS["DEBUG"],
        log_dir=tmp_path,
        log_file_name="custom.log",
        running_in_ngen=True,
    )

    cfg = load_config("TROUTE")

    assert cfg.enabled is False
    assert cfg.default_level == LEVELS["DEBUG"]
    assert cfg.log_dir == tmp_path
    assert cfg.log_file_name == "custom.log"
    assert cfg.running_in_ngen is True

    clear_runtime_override("TROUTE")
