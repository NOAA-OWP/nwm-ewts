import logging

import pytest

import ewts.config as config
import ewts.logger as logger


@pytest.fixture
def clean_ewts_env(monkeypatch):
    """Reset EWTS process-wide state between tests.

    The EWTS package intentionally keeps logger proxies, runtime overrides,
    initialization-print guards, and stdlib logging handlers at module scope.
    Tests need all of that cleared so one test's setup_logger()/bind() call does
    not affect later tests.
    """
    for name in (
        "EWTS_ENABLED",
        "EWTS_LOG_DIR",
        "EWTS_LOG_LEVEL",
        "EWTS_RANK",
        "EWTS_NGEN_BRIDGE_LIB",
        "EWTS_DEBUG",
        "TROUTE_LOGLEVEL",
        "NGEN_RESULTS_DIR",
    ):
        monkeypatch.setenv(name, "")

    logger._LOGGER_CACHE.clear()
    logger._init_printed.clear()
    config._RUNTIME_OVERRIDES.clear()

    for name in list(logging.root.manager.loggerDict.keys()):
        if name.startswith("ewts."):
            log = logging.getLogger(name)
            for handler in list(log.handlers):
                handler.close()
                log.removeHandler(handler)
            log.filters.clear()
            log.setLevel(logging.NOTSET)
            log.propagate = True
            log.disabled = False

    yield

    logger._LOGGER_CACHE.clear()
    logger._init_printed.clear()
    config._RUNTIME_OVERRIDES.clear()
