from setuptools import setup
from torch.utils.cpp_extension import BuildExtension, CppExtension

import os
os.environ['DISTUTILS_USE_SDK'] = '1'

setup(
    name='nnue_loader',
    ext_modules=[
        CppExtension(
            name='nnue_loader', 
            sources=['data_loader.cpp'],
            extra_compile_args=['-O3'] # Maximizes C++ speed
        )
    ],
    cmdclass={
        'build_ext': BuildExtension
    }
)