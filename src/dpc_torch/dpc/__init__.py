import torch

from ._C import (
    get_pinned_cores,
)

def pin_away():
    reserved = set(_C.get_pinned_cores())
    if not reserved:
        return
    allowed = os.sched_getaffinity(0)
    os.sched_setaffinity(0, allowed - reserved)
