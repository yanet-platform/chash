dependencies = [
	dependency('gtest'),
	dependency('gtest_main')
]

sources = files(
	'test-balancer.cpp'
)

balancer = executable(
	'balancer-unittest',
	sources,
	dependencies: dependencies
)

test('balancer', balancer, protocol: 'gtest')

bit_reverse = executable(
	'bit-reverse-unittest',
	'test-bit-reverse.cpp',
	dependencies: dependencies
)

test('bit-reverse', bit_reverse, protocol: 'gtest')

delta = executable(
	'delta-unittest',
	'test-delta.cpp',
	dependencies: dependencies
)

test('delta', delta, protocol: 'gtest')