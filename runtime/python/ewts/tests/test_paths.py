from pathlib import Path

from ewts.paths import default_run_logs_dir, make_log_path
import ewts.paths as paths_mod


def test_default_run_logs_dir(monkeypatch, tmp_path):
    monkeypatch.setattr(Path, "home", classmethod(lambda cls: tmp_path))
    assert default_run_logs_dir() == tmp_path / "run_logs"


def test_make_log_path_uses_compact_timestamp(monkeypatch, tmp_path):
    monkeypatch.setattr(paths_mod, "compact_utc_timestamp", lambda: "20260101T010203")

    p = make_log_path(tmp_path, "TROUTE")

    assert p == tmp_path / "TROUTE_20260101T010203.log"


def test_make_log_path_includes_rank_when_non_negative(monkeypatch, tmp_path):
    monkeypatch.setattr(paths_mod, "compact_utc_timestamp", lambda: "20260101T010203")

    p = make_log_path(tmp_path, "TROUTE", mpi_rank=3)

    assert p == tmp_path / "TROUTE_rank_3_20260101T010203.log"


def test_make_log_path_uses_custom_file_name(tmp_path):
    p = make_log_path(tmp_path, "TROUTE", mpi_rank=3, log_file_name="custom.log")

    assert p == tmp_path / "custom.log"


def test_make_log_path_defaults_blank_id_to_unknown(monkeypatch, tmp_path):
    monkeypatch.setattr(paths_mod, "compact_utc_timestamp", lambda: "20260101T010203")

    p = make_log_path(tmp_path, "")

    assert p == tmp_path / "UNKNOWN_20260101T010203.log"
