project('crc32', 'cpp')

crc32_inc = include_directories('include')
crc32_lib = library('libcrc32', 'Crc32.cpp', include_directories : crc32_inc)

crc32_dep = declare_dependency(link_with : crc32_lib, include_directories : crc32_inc)

