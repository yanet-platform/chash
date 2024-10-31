sources = files('main.cpp',
				'../common/Crc32.cpp')

cmake = import('cmake')
opt_var = cmake.subproject_options()
opt_var.set_override_option('cpp_std', disabler(), target: 'ARGPARSE_TESTS')

argparse = cmake.subproject('argparse', options: opt_var)
libargparse = argparse.dependency('argparse')

dependencies = []
dependencies += libargparse
dependencies += balancer_dep

executable('demo', sources, dependencies: dependencies)
