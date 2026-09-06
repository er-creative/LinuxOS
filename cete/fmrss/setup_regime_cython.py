import os
from pathlib import Path

import numpy as np
from Cython.Build import cythonize
from setuptools import Extension, setup


SCRIPT_DIRECTORY = Path(__file__).resolve().parent

# The script works from either the FMRSS root or its engines directory.
if SCRIPT_DIRECTORY.name == "engines":
    PROJECT_ROOT = SCRIPT_DIRECTORY.parent
    KERNEL_FILE = SCRIPT_DIRECTORY / "regime_rolling.pyx"
else:
    PROJECT_ROOT = SCRIPT_DIRECTORY
    KERNEL_FILE = PROJECT_ROOT / "engines" / "regime_rolling.pyx"

if not KERNEL_FILE.is_file():
    raise FileNotFoundError(
        "Cython source file not found:\n"
        f"{KERNEL_FILE}\n"
        "Place regime_rolling.pyx inside the engines directory."
    )

# Ensures --inplace writes engines/_regime_rolling*.so, regardless of the
# directory from which this build script was started.
os.chdir(PROJECT_ROOT)


extensions = [
    Extension(
        name="engines._regime_rolling",
        sources=[str(KERNEL_FILE)],
        include_dirs=[np.get_include()],
        extra_compile_args=["-O3"],
    )
]


setup(
    name="fmrss-regime-cython",
    version="1.0.0",
    description="Cython rolling regime-statistics kernel for FMRSS",
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
    zip_safe=False,
)
