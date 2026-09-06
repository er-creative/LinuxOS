import os
from pathlib import Path

import numpy as np
from Cython.Build import cythonize
from setuptools import Extension, setup


SCRIPT_DIRECTORY = Path(__file__).resolve().parent

# Support either recommended placement:
#   /home/devinderjeet/fmrss/setup_residual_cython.py
# or:
#   /home/devinderjeet/fmrss/engines/setup_residual_cython.py
if SCRIPT_DIRECTORY.name == "engines":
    PROJECT_ROOT = SCRIPT_DIRECTORY.parent
    KERNEL_FILE = SCRIPT_DIRECTORY / "rolling_robust.pyx"
else:
    PROJECT_ROOT = SCRIPT_DIRECTORY
    KERNEL_FILE = PROJECT_ROOT / "engines" / "rolling_robust.pyx"

if not KERNEL_FILE.is_file():
    raise FileNotFoundError(
        "Cython source file not found:\n"
        f"{KERNEL_FILE}\n"
        "Place rolling_robust.pyx inside the engines directory."
    )

# setuptools resolves --inplace output relative to the current directory.
# Moving to the project root prevents engines/engines path mistakes.
os.chdir(PROJECT_ROOT)


extensions = [
    Extension(
        name="engines._rolling_robust",
        sources=[str(KERNEL_FILE)],
        include_dirs=[np.get_include()],
        extra_compile_args=["-O3"],
    )
]


setup(
    name="fmrss-residual-cython",
    version="1.0.0",
    description=(
        "Cython rolling robust-statistics kernel for FMRSS"
    ),
    ext_modules=cythonize(
        extensions,
        compiler_directives={
            "language_level": "3",
            "boundscheck": False,
            "wraparound": False,
            "initializedcheck": False,
            "cdivision": True,
        },
        annotate=False,
    ),
    zip_safe=False
)
