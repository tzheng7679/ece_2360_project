from enum import Enum

BAD_KEYWORDS = [
    "error",
    "fail",
    "failed",
    "failure",
    "warning",
    "warn",
    "critical",
    "alert",
    "battery low",
    "danger",
]
GOOD_KEYWORDS = [
    "success",
    "complete",
    "completed",
    "finish",
    "finished",
    "done",
    "downloaded",
    "saved",
    "passed",
]
RED = [255, 0, 0]
GREEN = [0, 255, 0]
BLUE = [0, 0, 255]
PURPLE = [255, 0, 255]
WHITE = [255, 255, 255]

SILENT = [0]
FAILURE = [1]
SUCCESS = [2]
NEUTRAL = [3]


class Goodness(Enum):
    GOOD = 0
    BAD = 1
    NONE = 2

    @classmethod
    def of(cls, s: str):
        if any(good.lower() in s.lower() for good in GOOD_KEYWORDS):
            return cls.GOOD
        if any(bad.lower() in s.lower() for bad in BAD_KEYWORDS):
            return cls.BAD
        return cls.NONE
