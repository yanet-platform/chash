#include <fstream>
#include <iostream>
#include <string>

#include <argparse/argparse.hpp>

#include "chatty-chash.hpp"

std::map<std::string, chash::RealConfig> ReadConfig(std::string path)
{
	std::ifstream config(path);
	if (!config.is_open())
	{
		std::cout << "Failed to open config file.\n";
		std::exit(EXIT_FAILURE);
	}

	std::map<std::string, chash::RealConfig> result;
	for (std::string line; std::getline(config, line);)
	{
		std::string real;
		chash::RealId id;
		unsigned int weight;

		std::istringstream is(std::move(line));
		is >> real;
		is >> id;
		is >> weight;
		if (!is.eof())
		{
			std::cout << "Wrong config file format\n";
			std::exit(EXIT_FAILURE);
		}
		if (weight > std::numeric_limits<chash::Weight>::max())
		{
			std::cout << "Weight out of range\n";
			std::exit(EXIT_FAILURE);
		}
		result.emplace(real, chash::RealConfig{id, static_cast<chash::Weight>(weight)});
	}
	return result;
}

template<typename Real>
std::ostream& operator<<(std::ostream& o, const chash::Unweighted<Real>& ring)
{
	std::map<Real, chash::IdHash> to_hid;
	for (auto& [h, r] : ring.to_real_)
	{
		to_hid[r] = h;
	}
	o << "Salt: " << ring.salt_;
	o << " Unweighted: {";
	auto hid = to_hid.begin();
	auto print = [](std::ostream& o, decltype(hid) & i) {
		o << '{' << i->first << ", " << i->second << '}';
	};
	print(o, hid);
	++hid;
	for (auto end = to_hid.end(); hid != end; ++hid)
	{
		o << ", ";
		print(o, hid);
	}

	o << '}';
	return o;
}

int main(int argc, char* argv[])
{
	argparse::ArgumentParser args;
	args.add_argument("-c", "--config")
	        .help("path to configuration file. File should contain lines '<real_name_string> <real>\\n'.");
	args.add_argument("-l", "--lookup-bits")
	        .help("size of the lookup array. Actual value is 2^<this_argument>")
	        .implicit_value(16)
	        .scan<'u', std::uint8_t>();
	args.add_argument("-i", "--interactive")
	        .help("run interactive shell")
			.flag();
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

	std::map<std::string, chash::RealConfig> reals;

	if (std::optional<std::string> path = args.present("--config"))
	{
		reals = ReadConfig(path.value());
	}
	else
	{
		reals = {{"alpha", {0, 100}},
		         {"beta", {1, 100}},
		         {"gamma", {2, 50}},
		         {"delta", {3, 10}},
		         {"epsilon", {4, 1}}};
	}
	std::cout << "Demo start." << std::endl;

	std::pmr::unsynchronized_pool_resource mem;

	ChattyChash<std::string> bal(reals, 1 << 14, 14, &mem);

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
