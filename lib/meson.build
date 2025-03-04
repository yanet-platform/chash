chash_inc = include_directories('../lib')
sources = files(
	'hash.cpp',
	'utils.cpp',
	'../3rdparty/Crc32.cpp'
)

chashlib = library('chash', sources)

chash_dep = declare_dependency(link_with: chashlib, include_directories : chash_inc)

if get_option('tests')
	subdir('unittest')
endif
