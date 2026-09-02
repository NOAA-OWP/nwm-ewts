from ewts.formatter import EWTS_ID_WIDTH
from ewts.module_keys import ewts_id_from_key, keys_from_ewts_id


def test_ewts_id_width_is_8():
    assert EWTS_ID_WIDTH == 8


def test_module_key_lookup_and_duplicates_supported():
    assert ewts_id_from_key("t-route") == "TROUTE"
