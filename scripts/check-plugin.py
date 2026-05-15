#!/usr/bin/env python3
"""Smoke test: import DPC and instantiate classes as a user would."""
import torch
import dpc

print(f"torch {torch.__version__}")
print(f"dpc from {dpc.__file__}")
print(f"CUDA available: {torch.cuda.is_available()}")
if torch.cuda.is_available():
    print(f"CUDA device: {torch.cuda.get_device_name(0)}")
print("DPA Public API:")
for name in dpc.__all__:
    print(f"  dpc.{name}")

print()

# Instantiate option classes
# sock_opts = dpa.DPASocketBackendOptions()
print(f"{dpc.DPASocketBackendOptions()}")
print("----------------------------------------------------------")
print(f"{dpc.DPADpdkBackendOptions()}")
print("----------------------------------------------------------")
print(f"{dpc.DPADeviceOptions()}")
print("----------------------------------------------------------")
print(f"{dpc.ProcessGroupDPASocketOptions()}")
print("----------------------------------------------------------")
print(f"{dpc.ProcessGroupDPADpdkOptions()}")


print("\nOK.")

