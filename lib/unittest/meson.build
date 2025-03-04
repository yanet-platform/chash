dependencies = [
	dependency('gtest'),
	dependency('gtest_main'),
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

balancer = executable(
	'balancer-unittest',
	sources,
	dependencies: dependencies
)

test('balancer', balancer, protocol: 'gtest')

adjust = executable(
        'adjust-unittest',
        'test-adjust.cpp',
        dependencies: dependencies
)

test('adjust', adjust, protocol: 'gtest')

equal_weights = executable(
        'equal-weights-unittest',
        'test-equal-weights.cpp',
        dependencies: dependencies
)

test('equal-weights', equal_weights, protocol: 'gtest')
