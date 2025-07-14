project('chash', 'cpp', default_options: [
												'cpp_std=c++17'
											])

#add_global_arguments(['-Ofast'], language : 'cpp')

crc32 = subproject('crc32')

chash_inc = include_directories('include')
chash_dep = declare_dependency(dependencies : crc32.get_variable('crc32_dep'), include_directories : chash_inc)

subdir('demo')
