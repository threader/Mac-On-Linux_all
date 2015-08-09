#! /usr/bin/env python
from distutils.core import setup, Extension

ext_modules = [ 
	Extension('molimg', 
		sources=['py-mol-img.c', 'mol-img-lib.c'],
		### FIXME - hack for now...
		include_dirs=[ "../../src/include", "../../src/shared", "../../obj-ppc/include", "../../obj-osx/include" ],
		),
		
	]

setup(	name="molimg",
	version="1.0",
	ext_modules=ext_modules )
