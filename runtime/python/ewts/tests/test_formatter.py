import re

from ewts.formatter import (
    iso_utc_timestamp_ms,
    compact_utc_timestamp,
    pad_ewts_id,
    fixed_level_name,
    format_prefix,
    split_lines,
    EWTS_ID_WIDTH,
    LEVEL_WIDTH,
)
from ewts.log_levels import LEVELS


def test_iso_utc_timestamp_ms_format():
    ts = iso_utc_timestamp_ms()
    # YYYY-MM-DDTHH:MM:SS.mmmZ
    assert re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z", ts)


def test_compact_utc_timestamp_format():
    ts = compact_utc_timestamp()
    assert re.fullmatch(r"\d{8}T\d{6}", ts)


def test_pad_ewts_id_width_and_case():
    out = pad_ewts_id("abc")
    assert out == "ABC" + (" " * (EWTS_ID_WIDTH - 3))
    assert len(out) == EWTS_ID_WIDTH

    out2 = pad_ewts_id("ABCDEFGHIJK")
    assert out2 == "ABCDEFGH"
    assert len(out2) == EWTS_ID_WIDTH


def test_fixed_level_name_padding_and_error_maps_to_severe():
    out = fixed_level_name(LEVELS["INFO"])
    assert len(out) == LEVEL_WIDTH
    assert out.strip() == "INFO"

    out2 = fixed_level_name(LEVELS["SEVERE"])  # 40
    assert len(out2) == LEVEL_WIDTH
    assert out2.strip() == "SEVERE"


def test_format_prefix_shape():
    pfx = format_prefix("TROUTE", LEVELS["WARNING"])
    # format: "<ts> <8 chars> <7 chars>" (id and level include padding spaces)
    m = re.fullmatch(r"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z) (.{8}) (.{7})", pfx)
    assert m is not None
    ts, ewts_id, lvl = m.groups()
    assert re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z", ts)
    assert len(ewts_id) == EWTS_ID_WIDTH
    assert len(lvl) == LEVEL_WIDTH


def test_split_lines_behavior():
    assert split_lines("") == [""]
    assert split_lines("a\nb") == ["a", "b"]
    assert split_lines("a\n") == ["a"]
    assert split_lines(None) == [""]
