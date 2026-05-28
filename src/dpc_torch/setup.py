import glob
import os
import pprint
import shutil
import subprocess
import sys
import warnings
from pathlib import Path

import torch
from setuptools import Command, find_packages, setup

warnings.filterwarnings("ignore", message=".*easy_install command is deprecated.*")
warnings.filterwarnings("ignore", message=".*setup.py install is deprecated.*")
# from torch.utils import cpp_extension
# from distutils import log as distlog

from torch.utils.cpp_extension import CUDA_HOME as TORCH_CUDA_HOME
from torch.utils.cpp_extension import BuildExtension, CppExtension, include_paths

# distlog.set_verbosity(0)  # only show errors

PACKAGE_NAME = "dpc"
PACKAGE_VERSION = "0.0.1"

HERE = Path(__file__).parent.resolve()
BUILD_BASE = HERE.parent.parent / "build" / "plugin-build"
BUILD_BASE.mkdir(parents=True, exist_ok=True)

# setup_dir = os.path.dirname(os.path.abspath(__file__))
# os.chdir(setup_dir)
# src = ["ProcessGroupDPA.cpp", "ProcessGroupDPAWork.cpp", "ProcessGroupDPAUtils.cpp", "PythonBindings.cpp"]
# sources = list(map(lambda f: os.path.join(os.path.dirname(os.path.abspath(__file__)), f), src))

sources = sorted(glob.glob(str(HERE / "*.cpp"))) + sorted(glob.glob(str(HERE / "*.cc")))


def get_dpdk_config():
    """Get DPDK configuration from pkg-config"""

    def run_pkg_config(args):
        try:
            return subprocess.check_output(
                ["pkg-config"] + args, encoding="utf-8"
            ).strip()
        except:
            return ""

    libdir = run_pkg_config(["--variable=libdir", "libdpdk"])
    if not libdir:
        raise RuntimeError("DPDK not found. Set PKG_CONFIG_PATH to point to libdpdk.pc")

    includes = run_pkg_config(["--cflags-only-I", "libdpdk"])
    include_dirs = [flag[2:] for flag in includes.split() if flag.startswith("-I")]

    other = run_pkg_config(["--cflags-only-other", "libdpdk"]).split()
    other_cflags = [f for f in other if not f.startswith(("-march=", "-mtune="))]

    libs = run_pkg_config(["--libs-only-l", "libdpdk"])
    libraries = [lib[2:] for lib in libs.split() if lib.startswith("-l")]

    pmd_candidates = sorted(glob.glob(f"{libdir}/dpdk/pmds-*"))
    pmd_dir = pmd_candidates[-1] if pmd_candidates else ""

    # pmd_dir = os.path.join(libdir, "dpdk", "pmds-23.0")
    return {
        "libdir": libdir,
        "pmd_dir": pmd_dir,
        "include_dirs": include_dirs,
        "libraries": libraries,
        "other_cflags": other_cflags,
    }


# cuda_home = os.environ.get("CUDA_HOME", "/usr/local/cuda-12.6")


def find_cuda_home():
    if os.environ.get("CUDA_HOME"):
        return os.environ["CUDA_HOME"]
    if TORCH_CUDA_HOME:
        return TORCH_CUDA_HOME
    nvcc = shutil.which("nvcc")
    if nvcc:
        return os.path.dirname(os.path.dirname(nvcc))
    raise RuntimeError("CUDA not found. Install CUDA or set CUDA_HOME.")

dpdk_config = get_dpdk_config()

cuda_home = find_cuda_home()

print("DPDK_INCLUD:", dpdk_config["include_dirs"])
print("DPDK_LIBDIR:", dpdk_config["libdir"])
print("DPDK_PMDDIR:", dpdk_config["pmd_dir"])
print("DPDK_CFLAGS:", dpdk_config["other_cflags"])
print("CUDA_HOME:", cuda_home)


dpc_build = os.path.abspath(os.path.join(HERE, "..", "..", "build"))
dpc_source = os.path.abspath(os.path.join(HERE, "..", "..", "src"))
dpc_install = os.environ.get("DPC_INSTALL", os.path.join(dpc_build, "install"))
# os.path.abspath(os.path.join(HERE, "..", "..", "build", "install"))

torch_includes = include_paths()
print("TORCH_INCLUDES:", torch_includes)

include_dirs = [
    str(HERE),
    os.path.join(dpc_install, "include"),
    dpc_source,
    os.path.join(cuda_home, "include"),
    dpdk_config["include_dirs"],
    torch_includes
]
library_dirs = [
    # os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")),
    os.path.join(dpc_install, "lib"),
    os.path.join(dpc_build, "lib"),
    # *dpdk_libdirs
]

is_develop = (
    "develop" in sys.argv
)  # or "-e" in sys.argv  # pip install -e . ends up here

define_macros = [("DPC_PYTHON", "1")]
cxx_args = [
    "-fvisibility=hidden",
    "-fvisibility-inlines-hidden",
    "-march=native",
    "-O3",
    "-flto",
]
cuda_args = ["-O3"]

if torch.cuda.is_available():
    define_macros.append(("DPC_CUDA", "1"))

ext = CppExtension(
    name="dpc._C",
    sources=sources,
    include_dirs=include_dirs,
    # + dpdk_config["include_dirs"]
    # + include_paths(),  # include_dirs, #+ d
    libraries=["dpc"],  # Don't duplicate DPDK libs here
    library_dirs=library_dirs + [dpdk_config["libdir"]],  # library_dirs, #+
    define_macros=define_macros,
    extra_compile_args={"cxx": cxx_args},
    extra_link_args=[
        "-Wl,--disable-new-dtags",
        f"-Wl,-rpath,{dpdk_config['libdir']}",
        f"-Wl,-rpath,{dpdk_config['pmd_dir']}",
        "-Wl,--no-as-needed",
        # "-lrte_net_mlx5",
        # "-lrte_common_mlx5",
        # "-lmlx5"
    ]
    + [f"-l{lib}" for lib in dpdk_config["libraries"]]
    + ["-Wl,--as-needed"],
)


class UninstallCommand(Command):
    """Custom command to uninstall the built extension."""

    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        import importlib.util
        import site

        name = PACKAGE_NAME
        spec = importlib.util.find_spec(name)
        if spec is not None and spec.origin:
            print(f"Removing {spec.origin}")
            os.remove(spec.origin)
            # Also try to remove from .egg-link if in develop mode
            for path in site.getsitepackages() + [site.getusersitepackages()]:
                egg_link = os.path.join(path, f"{name}.egg-link")
                if os.path.isfile(egg_link):
                    print(f"Removing {egg_link}")
                    os.remove(egg_link)
        else:
            print(f"Module {name} not found. Nothing to remove.")


setup(
    name=PACKAGE_NAME,
    version=PACKAGE_VERSION,
    packages=find_packages(include=["dpc*"]),
    ext_modules=[ext],
    cmdclass={"build_ext": BuildExtension, "uninstall": UninstallCommand},
    zip_safe=False,
    options={
        "build": {"build_base": str(BUILD_BASE)},
        "egg_info": {"egg_base": str(BUILD_BASE)},
    },
)
