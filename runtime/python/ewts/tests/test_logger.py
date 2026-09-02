import logging
import re

import ewts.formatter as formatter
import ewts.paths as paths
from ewts import configure_existing_logger, get_logger, setup_logger
from ewts.log_levels import LEVELS


def test_get_logger_caches_initialized_logger(clean_ewts_env):
    a = get_logger("t-route")
    b = get_logger("TROUTE")

    assert a is b
    assert a.ewts_id == "TROUTE"


def test_get_logger_returns_real_logger_immediately(clean_ewts_env):
    lg = get_logger("TROUTE")

    assert lg.ewts_id == "TROUTE"
    assert hasattr(lg, "info")
    assert lg.hasHandlers()


def test_logger_defaults_to_stdout_when_no_log_dir(clean_ewts_env, monkeypatch, capsys, tmp_path):
    monkeypatch.delenv("EWTS_LOG_DIR", raising=False)
    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(formatter, "iso_utc_timestamp_ms", lambda: "2026-01-01T01:02:03.004Z")

    lg = get_logger("TROUTE")

    lg.info("hello stdout")

    captured = capsys.readouterr()

    assert "EWTS TROUTE using stdout logging" in captured.out
    assert "2026-01-01T01:02:03.004Z TROUTE" in captured.out
    assert "INFO" in captured.out
    assert "hello stdout" in captured.out
    assert not list(tmp_path.glob("*.log"))
    assert not (tmp_path / "run_logs").exists()


def test_logger_writes_file_with_expected_prefix_when_log_dir_set(clean_ewts_env, monkeypatch, tmp_path, capsys):
    monkeypatch.setenv("EWTS_LOG_DIR", str(tmp_path))

    monkeypatch.setattr(formatter, "iso_utc_timestamp_ms", lambda: "2026-01-01T01:02:03.004Z")
    monkeypatch.setattr(paths, "compact_utc_timestamp", lambda: "20260101T010203")

    lg = get_logger("TROUTE")

    lg.info("hello")

    files = list(tmp_path.glob("TROUTE_*.log"))
    assert len(files) == 1

    line = files[0].read_text(encoding="utf-8").splitlines()[0]

    assert line.startswith("2026-01-01T01:02:03.004Z ")
    assert "TROUTE" in line
    assert re.search(r"\bINFO\b", line)
    assert line.rstrip().endswith("hello")
    assert "hello" not in capsys.readouterr().out.split("logging to")[-1]


def test_logger_uses_custom_log_file_name_from_setup_logger(clean_ewts_env, monkeypatch, tmp_path):
    monkeypatch.setattr(formatter, "iso_utc_timestamp_ms", lambda: "2026-01-01T01:02:03.004Z")

    lg = setup_logger(
        "TROUTE",
        log_dir=tmp_path,
        log_file_name="custom.log",
    )

    lg.info("custom file message")

    log_file = tmp_path / "custom.log"
    assert log_file.exists()
    assert "custom file message" in log_file.read_text(encoding="utf-8")


def test_logger_includes_rank_in_default_file_name(clean_ewts_env, monkeypatch, tmp_path):
    monkeypatch.setenv("EWTS_LOG_DIR", str(tmp_path))
    monkeypatch.setenv("EWTS_RANK", "7")
    monkeypatch.setattr(paths, "compact_utc_timestamp", lambda: "20260101T010203")

    lg = get_logger("TROUTE")
    lg.info("ranked message")

    files = list(tmp_path.glob("TROUTE_rank_7_*.log"))
    assert len(files) == 1


def test_perform_level(clean_ewts_env, monkeypatch, tmp_path):
    monkeypatch.setenv("EWTS_LOG_DIR", str(tmp_path))
    monkeypatch.setenv("EWTS_LOG_LEVEL", "PERFORM")

    monkeypatch.setattr(formatter, "iso_utc_timestamp_ms", lambda: "2026-01-01T01:02:03.004Z")
    monkeypatch.setattr(paths, "compact_utc_timestamp", lambda: "20260101T010203")

    lg = get_logger("TROUTE")

    lg.perform("perf message")

    files = list(tmp_path.glob("TROUTE_*.log"))
    assert len(files) == 1

    line = files[0].read_text().splitlines()[0]

    assert "PERFORM" in line


def test_info_not_mapped_to_perform(clean_ewts_env, monkeypatch, tmp_path):
    monkeypatch.setenv("EWTS_LOG_DIR", str(tmp_path))

    monkeypatch.setattr(formatter, "iso_utc_timestamp_ms", lambda: "2026-01-01T01:02:03.004Z")
    monkeypatch.setattr(paths, "compact_utc_timestamp", lambda: "20260101T010203")

    lg = get_logger("TROUTE")

    lg.info("info message")

    files = list(tmp_path.glob("TROUTE_*.log"))
    assert len(files) == 1

    line = files[0].read_text().splitlines()[0]

    assert "INFO" in line
    assert "PERFORM" not in line


def test_messages_below_configured_level_are_filtered(clean_ewts_env, monkeypatch, capsys):
    monkeypatch.delenv("EWTS_LOG_DIR", raising=False)
    monkeypatch.setenv("EWTS_LOG_LEVEL", "WARNING")

    lg = get_logger("TROUTE")

    lg.info("do not show")
    lg.warning("show warning")

    out = capsys.readouterr().out
    assert "do not show" not in out
    assert "show warning" in out


def test_disabled_logger_suppresses_messages(clean_ewts_env, monkeypatch, capsys):
    monkeypatch.setenv("EWTS_ENABLED", "false")

    lg = get_logger("TROUTE")
    lg.fatal("do not log")

    out = capsys.readouterr().out
    assert "logging is DISABLED" in out
    assert "do not log" not in out


def test_logger_supports_additional_python_handler(clean_ewts_env):
    seen = []

    class ListHandler(logging.Handler):
        def emit(self, record):
            seen.append(record.getMessage())

    lg = get_logger("TROUTE")

    lg.addHandler(ListHandler())

    lg.info("hello from extra handler")

    assert "hello from extra handler" in seen


def test_configure_existing_logger_replaces_handlers(clean_ewts_env, monkeypatch, tmp_path):
    monkeypatch.setenv("EWTS_LOG_DIR", str(tmp_path))
    monkeypatch.setattr(paths, "compact_utc_timestamp", lambda: "20260101T010203")

    app_logger = logging.getLogger("TROUTE")
    old_handler = logging.NullHandler()
    app_logger.addHandler(old_handler)
    app_logger.propagate = True

    configured = configure_existing_logger(app_logger)
    configured.info("configured logger message")

    assert configured is app_logger
    assert old_handler not in configured.handlers
    assert configured.propagate is False
    assert len(configured.handlers) == 1

    files = list(tmp_path.glob("TROUTE_*.log"))
    assert len(files) == 1
    assert "configured logger message" in files[0].read_text(encoding="utf-8")


def test_configure_existing_logger_rejects_unknown_logger_name(clean_ewts_env):
    app_logger = logging.getLogger("troute")

    try:
        configure_existing_logger(app_logger)
        assert False, "Expected ValueError for lowercase/unknown EWTS id"
    except ValueError as exc:
        assert "known EWTS module id" in str(exc)
