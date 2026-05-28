import torch
import os

from ._C import (
    Options,
    get_pinned_cores,
    get_available_cores
)

def pin_away():
  allowed = os.sched_getaffinity(0)
  reserved = set(get_pinned_cores())
  print("reserved", get_pinned_cores())
  print("allowed", allowed)
  print("avail", get_available_cores())
  if not reserved:
      return get_available_cores()
  leftover = allowed - reserved
  print("leftover", leftover)
  os.sched_setaffinity(0, allowed - reserved)
  return leftover
