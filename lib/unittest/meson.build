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
	dependencies: dependencies)

test('balancer', balancer, protocol: 'gtest')

ring_iter = executable(
	'ring-iter-unittest',
	'test-ring-iterator.cpp',
	dependencies: dependencies)

test('ring-iterator', ring_iter, protocol: 'gtest')

