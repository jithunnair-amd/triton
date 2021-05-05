import os
import re
import sys
import sysconfig
import platform
import subprocess
import distutils
import glob
import tempfile
from distutils.version import LooseVersion
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext
from setuptools.command.test import test as TestCommand
import distutils.spawn


class CMakeExtension(Extension):
    def __init__(self, name, path, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)
        self.path = path


class CMakeBuild(build_ext):

    user_options = build_ext.user_options + [('base-dir=', None, 'base directory of Triton')]

    def initialize_options(self):
        print("CMakeBuild:initialize_options")
        build_ext.initialize_options(self)
        self.base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))

    def finalize_options(self):
        print("CMakeBuild:finalize_options")
        build_ext.finalize_options(self)

    def run(self):
        print("CMakeBuild:run")
        try:
            out = subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError(
                "CMake must be installed to build the following extensions: " + ", ".join(e.name for e in self.extensions)
            )

        if platform.system() == "Windows":
            cmake_version = LooseVersion(re.search(r"version\s*([\d.]+)", out.decode()).group(1))
            if cmake_version < "3.1.0":
                raise RuntimeError("CMake >= 3.1.0 is required on Windows")

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        # self.debug = True
        self.debug = False
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.path)))
        # create build directories
        llvm_build_dir = os.path.join(tempfile.gettempdir(), "llvm")
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        if not os.path.exists(llvm_build_dir):
            os.makedirs(llvm_build_dir)
        # python directories
        python_include_dirs = [distutils.sysconfig.get_python_inc()] + ['/usr/local/cuda/include','/opt/rocm/include']
        print("python_include_dirs",python_include_dirs)
        python_lib_dirs = distutils.sysconfig.get_config_var("LIBDIR")
        print("python_lib_dirs",python_lib_dirs)
        cmake_args = [
            "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=" + extdir,
            "-DBUILD_TUTORIALS=OFF",
            "-DBUILD_PYTHON_MODULE=ON",
            #'-DPYTHON_EXECUTABLE=' + sys.executable,
            #'-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON,
            "-DTRITON_LLVM_BUILD_DIR=" + llvm_build_dir,
            "-DPYTHON_INCLUDE_DIRS=" + ";".join(python_include_dirs)
        ]
        # configuration
        cfg = "Debug" if self.debug else "Release"
        build_args = ["--config", cfg]

        if platform.system() == "Windows":
            cmake_args += ["-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), extdir)]
            if sys.maxsize > 2**32:
                cmake_args += ["-A", "x64"]
            build_args += ["--", "/m"]
        else:
            cmake_args += ["-DCMAKE_BUILD_TYPE=" + cfg]
            build_args += ["--", "-j8"]

        env = os.environ.copy()
        print("env", env)
        print("self.base_dir", self.base_dir)
        print("=self.build_temp", self.build_temp)
        print("cmake_args", cmake_args)
        print("build_args", build_args)
        subprocess.check_call(["cmake", self.base_dir] + cmake_args, cwd=self.build_temp, env=env)
        subprocess.check_call(["cmake", "--build", "."] + build_args, cwd=self.build_temp)


setup(
    name="triton",
    version="1.0.0",
    author="Philippe Tillet",
    author_email="phil@openai.com",
    description="A language and compiler for custom Deep Learning operations",
    long_description="",
    packages=["triton", "triton/_C", "triton/ops", "triton/ops/blocksparse"],
    install_requires=["numpy", "torch"],
    package_data={"triton/ops": ["*.c"], "triton/ops/blocksparse": ["*.c"]},
    include_package_data=True,
    ext_modules=[CMakeExtension("triton", "triton/_C/")],
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
    # for PyPI
    keywords=["Compiler", "Deep Learning"],
    url="https://github.com/ptillet/triton/",
    download_url="https://github.com/ptillet/triton/archive/v0.1.tar.gz",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Build Tools",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3.6",
    ],
)
