"""EWTS Python logging helpers.

Public API:
  - get_logger(module_key_or_ewts_id) -> EwtsLogger
  - EwtsLogger methods: debug/info/warning/error/severe/fatal/perform/log
"""
# This unused-looking import is deliberate — it’s part of the public API.
from ._version import __version__, NGWPC_VERSION
from .logger import EwtsLogger, setup_logger, get_logger, reset_logger, configure_existing_logger # noqa: F401
from .data_payloads import (  # noqa: F401
    LogParts,
    Payload,
    Status,
    parts_of_log_line,
    payload_of_log_msg,
)
from . import modules as _modules

# Re-export all *_ID constants from ewts.modules
for _name in dir(_modules):
    if _name.endswith("_ID") and _name.isupper():
        globals()[_name] = getattr(_modules, _name)

__all__ = (
    ["__version__", "NGWPC_VERSION", "EwtsLogger", "get_logger", "setup_logger", "get_logger", "reset_logger", "configure_existing_logger",
    "LogParts",
    "Payload",
    "Status",
    "parts_of_log_line",
    "payload_of_log_msg",
]
    + [
        _name
        for _name in dir(_modules)
        if _name.endswith("_ID") and _name.isupper()
    ]
)
