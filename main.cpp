#include <iostream>

#define DEBUG 1

#include "balancer.h"
//#include "salty_rings.h"
#include "combined.hpp"

int main(void)
{
	for (auto pos: BitReverseSequence<4>{})
	{
		std::cout << pos << ' ';
	}
	std:: cout << "\n------------------------------\n";
	std::cout << "Demo start." << std::endl;
	std::map<std::string, balancer::Weight> reals = {
	        {"alpha", 100}, {"beta", 100}, {"gamma", 50}, {"delta", 50}};
	Print(reals);
	std::cout << std::endl;
	balancer::Combined<std::string, 20> bal(reals, 10000);

	bal.Report();

	return 0;
}
