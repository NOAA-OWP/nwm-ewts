"""Classes to enable sending structured data payloads through log file entries.
Some of the syntax would be simpler with pydantic, but that libray was not part
of nwm-ewts dependencies when this was written."""

import json
import re
from dataclasses import InitVar, asdict, dataclass
from datetime import datetime, timezone
from enum import StrEnum

MSG_PAYLOAD_SENTINEL_START = "<MSG_DATA>"
MSG_PAYLOAD_SENTINEL_END = "</MSG_DATA>"
EXTRACT_PATTERN = re.compile(
    rf"{MSG_PAYLOAD_SENTINEL_START}(.*?){MSG_PAYLOAD_SENTINEL_END}"
)


class Status(StrEnum):
    """Status enum for log JSON payload."""

    NULL = "NULL"
    INITTING = "INITIALIZING"
    INITTED = "INITIALIZED"
    STARTING = "STARTING"
    INPROG = "IN_PROGRESS"
    COMPLETE = "COMPLETE"
    ERROR = "ERROR"


class LogPartsFactoryParserError(Exception):
    """Custom exception for errors encountered when parsing log lines into LogParts.
    Raised by payload_of_log_msg function."""


@dataclass
class Payload:
    """Log payload class for status reporting via logs.

    Parameters
    ----------
    status : Status
        Module status.
    prog : float, optional
        Module progress (0.0 to 1.0), by default None.
    msg : str, optional
        Message.
    modnm : str, optional
        Module name.

    Raises
    ----------
    ValueError
        If there is a validation error with the provided inputs.
    """

    status: Status
    prog: float | None = None
    msg: str | None = None
    modnm: str | None = None

    def __post_init__(self):
        errs: list[Exception] = []
        if not isinstance(self.status, Status):
            errs.append(TypeError(f"status: expect {Status}, got {type(self.status)}"))
        if isinstance(self.prog, float):
            if not (0.0 <= self.prog <= 1.0):
                errs.append(
                    ValueError(
                        f"prog: expect value between 0.0 and 1.0, got {self.prog}"
                    )
                )
        elif self.prog is not None:
            errs.append(
                TypeError(f"prog: expect {float} or None, got {type(self.prog)}")
            )
        if not isinstance(self.msg, (str, type(None))):
            errs.append(TypeError(f"msg: expect {str} or None, got {type(self.msg)}"))
        if not isinstance(self.modnm, (str, type(None))):
            errs.append(
                TypeError(f"modnm: expect {str} or None, got {type(self.modnm)}")
            )
        if errs:
            raise ValueError(f"Errors constructing Payload: {errs}")

    @property
    def json(self) -> str:
        """A json string (dict) representation for logging."""
        return json.dumps(asdict(self))

    @property
    def json_wrapped(self) -> str:
        """A json string (dict) representation for logging, wrapped with sentinel strings."""
        return f"{MSG_PAYLOAD_SENTINEL_START}{self.json}{MSG_PAYLOAD_SENTINEL_END}"

    def __str__(self) -> str:
        """String representation of the Payload. JSON wrapped with sentinel strings for logging."""
        return self.json_wrapped

    def __format__(self, format_spec) -> str:
        """String representation of the Payload. JSON wrapped with sentinel strings for logging."""
        return self.__str__()


def payload_of_log_msg(log_msg: str) -> Payload | None:
    """Factory for Payload object.

    Construct and return a Payload from a log message, if it contains the sentinel. Otherwise, return None.
    Requires that the provided string is one line (Payloads should have escape newline chars via json.dumps(asdict(self))).

    If "prog" is found and is not None/null, it will be cast to float.
    This allows prog to be stored as int for values 0 and 1 in the serialized JSON string.

    Parameters
    ----------
    log_msg : str
        The log message to extract the payload from.

    Returns
    -------
    Payload | None
        The extracted Payload object if sentinel wrapping found, else None.

    Raises
    ----------
    ValueError
        If the log message contains a sentinel string wrapping but the content between cannot be parsed into a Payload instance.
        If the log message contains multiple sentinel string wrappings.
    """
    matches = EXTRACT_PATTERN.findall(log_msg)
    if matches:
        if len(matches) != 1:
            raise ValueError(
                f"{len(matches)} payloads detected in log message. Expected 1. Full message: {log_msg}"
            )
        payload_raw_str = matches[0]
        try:
            d = json.loads(payload_raw_str)
            d["status"] = Status(d["status"])
            if "prog" in d and d["prog"] is not None:
                if isinstance(d["prog"], (int, float)):
                    d["prog"] = float(d["prog"])
                else:
                    raise ValueError(
                        f"Expected prog to be None, int, or float, but got: {type(d['prog'])}: {d['prog']}"
                    )
            payload = Payload(**d)
        except Exception as e:
            raise ValueError(
                f"Payload was detected in log message, but it failed to parse. Full message: {repr(log_msg)}. Payload raw string: {repr(payload_raw_str)}. Exception: {e}"
            ) from e
        return payload
    else:
        return None


@dataclass
class LogParts:
    """Parts of a log line, optionally with structured Payload substructure.

    Parameters
    ----------
    dt : datetime
        The timestamp of the log line.
    module : str
        The module that sent the message.
    level : str
        The log level of the message.
    msg : str
        The message (may include raw payload string).
    payload : Payload | None
        The extracted Payload if the message contains a structured payload (wrapped in sentinel strings), else None.
    tolerant : bool, optional
        If True, then types may be None, except for the payload attr which is always type-checked.
        This is to support messages that may not have a compliant structure but may contain a Payload to be parsed.
        Note, tolerant is not an attribute of the dataclass, it is an initialization variable (it does not get serialized).

    Raises
    ----------
    ValueError
        If there is a validation error with the provided inputs.
    """

    dt: datetime
    module: str
    level: str
    msg: str
    payload: Payload | None
    tolerant: InitVar[bool] = False

    def __post_init__(self, tolerant: bool):
        t = tolerant
        errs: list[Exception] = []
        if not isinstance(self.dt, datetime) and not (t and self.dt is None):
            errs.append(
                TypeError(f"dt: expect {datetime}, got {type(self.dt)} (tolerant={t})")
            )
        if not isinstance(self.module, str) and not (t and self.module is None):
            errs.append(
                TypeError(
                    f"module: expect {str}, got {type(self.module)} (tolerant={t})"
                )
            )
        if not isinstance(self.level, str) and not (t and self.level is None):
            errs.append(
                TypeError(f"level: expect {str}, got {type(self.level)} (tolerant={t})")
            )
        if not isinstance(self.msg, str) and not (t and self.msg is None):
            errs.append(
                TypeError(f"msg: expect {str}, got {type(self.msg)} (tolerant={t})")
            )
        if not isinstance(self.payload, (Payload, type(None))):
            errs.append(
                TypeError(
                    f"payload: expect {Payload} or None, got {type(self.payload)} (tolerant={t})"
                )
            )
        if errs:
            raise ValueError(f"Errors constructing LogParts: {errs}")


def parts_of_log_line(line: str, tolerant: bool = False) -> LogParts:
    """Factory for LogParts object.

    Construct and return a LogParts instance by parsing a log line (split on whitespace).
    Asserts that the log line has at least 4 whitespace-delimited parts:
    timestamp (datetime ISO string), module name, log level name, message.

    The message may contain whitespace (it is at the end).

    The message may include a Payload JSON string wrapped in the sentinel strings
    MSG_PAYLOAD_SENTINEL_START and MSG_PAYLOAD_SENTINEL_END. If it does include the
    sentinel strings, then the payload JSON dictionary between them will be parsed into
    a Payload instance and included as attribute of the returned LogParts. If not, the
    payload attribute of the returned LogParts will be None.

    If the line does not have an expected pattern, it will raise a LogPartsFactoryParserError,
    unless ``tolerant`` is True.

    Parameters
    ----------
    line : str
        The log line to parse.
    tolerant : bool, optional (default False)
        If True, parsing errors will not raise exceptions, but will return a LogParts instance with None for certain fields.
        If a payload sentinel exists in the line, it will be parsed. There is no tolerance for payloads (if they exist, they must be valid).

    Returns
    -------
    LogParts
        The constructed LogParts instance.

    Raises
    ----------
    LogPartsFactoryParserError
        If the line has less than 4 parts after splitting on whitespace.
        If the timestamp part does not use UTC timezone.
        If the line fails to parse for any other reason.
    """
    log_parts_kwargs = {
        "dt": None,
        "module": None,
        "level": None,
        "msg": None,
        "payload": None,
        "tolerant": tolerant,
    }
    parts = line.split(None, 3)
    if len(parts) < 4:
        if not tolerant:
            raise LogPartsFactoryParserError(f"Could not parse log line: {repr(line)}")
    else:
        timestamp_str, module, level, msg = parts
        try:
            dt = datetime.fromisoformat(timestamp_str)
            log_parts_kwargs.update({"module": module, "level": level, "msg": msg})
            if dt.tzinfo is None or dt.tzinfo != timezone.utc:
                raise ValueError(
                    f"Expected timezone {timezone.utc}, got: {dt.tzinfo}. Full line: {repr(line)}"
                )
            else:
                log_parts_kwargs.update({"dt": dt})
        except ValueError as e:
            if not tolerant:
                raise LogPartsFactoryParserError(
                    f"Error parsing line: {repr(line)}"
                ) from e

    log_parts_kwargs.update({"payload": payload_of_log_msg(line)})
    return LogParts(**log_parts_kwargs)
