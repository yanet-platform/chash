gtest_proj = subproject('gtest')
gtest_dep = gtest_proj.get_variable('gtest_dep')
gtest_main_dep = gtest_proj.get_variable('gtest_main_dep')

dependencies = [
	gtest_dep,
	gtest_main_dep,
	chash_dep
]


sources = files(
	'test-balancer.cpp'
)

bit_reverse = executable(
	'bit-reverse-unittest',
	'test-bit-reverse.cpp',
	dependencies: dependencies
)

test('bit-reverse', bit_reverse, protocol: 'gtest')

#unweighted = executable(
#	'unweighted-unittest',
#	'test-unweighted.cpp',
#	cpp_args: ['-g', '-O0'],
#	dependencies: dependencies
#)
#
#test('unweighted', unweighted, protocol: 'gtest')

#basic_test = executable(
#  'basic-unittest',
#  'test-basic.cpp',
#  dependencies: dependencies,
#  cpp_args: ['-g', '-O0']
#)

#test('basic', basic_test, protocol: 'gtest')

#service = executable(
#	'service-unittest',
#	'test-service.cpp',
#	cpp_args: ['-g', '-O0'],
#	dependencies: dependencies
#)
#
#test('service', service, protocol: 'gtest')

#balancer = executable(
#	'balancer-unittest',
#	sources,
#	cpp_args: ['-g', '-O0'],
#	dependencies: dependencies
#)
#
#test('balancer', balancer, protocol: 'gtest')

#equal_weights = executable(
#        'equal-weights-unittest',
#        'test-equal-weights.cpp',
#        dependencies: dependencies
#)

#test('equal-weights', equal_weights, protocol: 'gtest')

#speed = executable(
#	'speed-unittest',
#	'test-speed.cpp',
#	dependencies: dependencies
#)
#
#test('speed', speed, protocol: 'gtest')