#include <fstream>
#include <iostream>
#include <string>

#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

#include "chatty-chash.hpp"

std::map<std::string, balancer::Weight> ReadConfig(std::string path)
{
	std::ifstream config(path);
	if (!config.is_open())
	{
		std::cout << "Failed to open config file.\n";
		std::exit(EXIT_FAILURE);
	}

	nlohmann::json result = nlohmann::json::parse(config, nullptr, false);
	if (result.is_discarded())
	{
		std::cout << "Failed to parse configuration file: malformed JSON\n";
		std::exit(EXIT_FAILURE);
	}
	return result;
}

int main(int argc, char* argv[])
{
	argparse::ArgumentParser args;
	args.add_argument("-c", "--config")
	        .help("path to configuration file");
	args.add_argument("-l", "--lookup-bits")
	        .help("size of the lookup array. Actual value is 2^<this_argument>")
	        .implicit_value(16)
	        .scan<'u', std::uint8_t>();
	args.add_description("Standalone demo of consistent hashing library.");
	// args.add_epilog("TODO link to github page");

	try
	{
		args.parse_args(argc, argv);
	}
	catch (std::runtime_error& e)
	{
		std::cout << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}

	std::map<std::string, balancer::Weight> reals;

	if (std::optional<std::string> path = args.present("--config"))
	{
		reals = ReadConfig(path.value());
	}
	else
	{
		reals = {{"alpha", 100},
		         {"beta", 100},
		         {"gamma", 50},
		         {"delta", 10},
		         {"epsilon", 1}};
	}
	std::cout << "Demo start." << std::endl;
	std::cout << std::endl;
	ChattyChash<std::string, 18> bal(reals, 1 << 17);

	std::cout << "-----------------------------------------------------------\n"
	          << "  Reals requested\n"
	          << "-----------------------------------------------------------\n";
	Print(reals);
	std::cout << "\n";
	bal.ReportSettings();
	std::cout << "-----------------------------------------------------------\n"
	          << "  Real statistics\n"
	          << "-----------------------------------------------------------\n";
	bal.Report();
	std::cout << '\n';
	std::cout << "-----------------------------------------------------------\n"
	          << "  Fragmentation report (<segment_size>x<count>) per real\n"
	          << "-----------------------------------------------------------\n";
	bal.ReportFrag();

	return 0;
}
