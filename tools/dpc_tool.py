import dpc
import torch.distributed as dist
import os

os.environ["MASTER_ADDR"] = "localhost"
os.environ["MASTER_PORT"] = "29500"

dpc.get_available_cores()

dist.init_process_group("dpc", rank=0, world_size=1)

print(dpc.pin_away())
