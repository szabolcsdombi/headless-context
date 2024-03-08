from setuptools import Extension, setup

ext = Extension(
    name='headless_context',
    sources=['headless_context.cpp'],
    libraries=['user32', 'gdi32', 'opengl32'],
)

setup(
    name='headless-context',
    version='0.9.0',
    ext_modules=[ext],
)
