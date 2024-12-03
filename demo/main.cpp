#include <fstream>
#include <iostream>
#include <string>

#include <argparse/argparse.hpp>

#include "chash.hpp"
#include "report.hpp"

std::tuple<std::vector<std::string>,
           std::vector<std::uint32_t>,
           std::vector<std::uint32_t>>
ReadConfig(std::string path)
{
	std::ifstream config(path);
	if (!config.is_open())
	{
		std::cout << "Failed to open config file.\n";
		std::exit(EXIT_FAILURE);
	}

	std::tuple<std::vector<std::string>,
	           std::vector<std::uint32_t>,
	           std::vector<std::uint32_t>>
	        result;
	std::back_insert_iterator back_real(std::get<0>(result));
	std::back_insert_iterator back_id(std::get<1>(result));
	std::back_insert_iterator back_weight(std::get<2>(result));

	for (std::string line; std::getline(config, line);)
	{
		std::string real;
		std::uint32_t id;
		std::uint32_t weight;
		std::istringstream is(std::move(line));
		is >> real;
		is >> id;
		is >> weight;
		if (!is.eof())
		{
			std::cout << "Wrong config file format\n";
			std::exit(EXIT_FAILURE);
		}
		if (weight > 100)
		{
			std::cout << "Weight out of range\n";
			std::exit(EXIT_FAILURE);
		}
	}
	return result;
}

template<typename Real>
std::ostream& operator<<(std::ostream& o, const chash::Unweighted<Real>& ring)
{
	std::map<Real, chash::IdHash> to_hid;
	for (auto& [h, r] : ring.to_id_)
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

	std::vector<std::string> reals;
	std::vector<std::uint32_t> ids;
	std::vector<std::uint32_t> weights;

	if (std::optional<std::string> path = args.present("--config"))
	{
		std::tie(reals, ids, weights) = ReadConfig(path.value());
	}
	else
	{
		reals = {"alpha", "beta", "gamma", "delta", "epsilon"};
		ids = {0, 1, 2, 3, 4};
		weights = {100, 100, 50, 10, 1};
	}
	std::cout << "Demo start." << std::endl;

	std::vector<std::pair<std::string, std::uint32_t>> real_ids;
	for (std::size_t i = 0; i < reals.size(); ++i)
	{
		real_ids.emplace_back(reals.at(i), ids.at(i));
	}
	std::cout << "-----------------------------------------------------------\n"
	          << "  Reals requested\n"
	          << "-----------------------------------------------------------\n";
	Print(real_ids);
	std::cout << '\n';

	auto oupdater = chash::MakeWeightUpdater(real_ids, 10000, 20);

	if (!oupdater)
	{
		std::cout << "Failed to build updater\n";
		std::exit(EXIT_FAILURE);
	}
	auto& updater = oupdater.value();

	std::vector<std::uint32_t> lookup(updater.LookupSize(), std::numeric_limits<std::uint32_t>::max());

	updater.InitLookup(lookup.data());

	std::cout << "\n"
	             "-----------------------------------------------------------\n"
	             "  Init statistics\n"
	             "-----------------------------------------------------------\n";

	std::vector<std::pair<std::uint32_t, std::uint32_t>> ids_weights;
	for (std::size_t i=0; i< reals.size(); ++i)
	{
		ids_weights.emplace_back(ids.at(i), weights.at(i));
	}

	std::cout << "weights: ";
	report::Weights(lookup);
	std::cout << "clumps: ";
	report::Clumps(lookup);
	std::cout << "\n";
	std::cout << "-----------------------------------------------------------\n"
	          << "  Apply weights\n"
	          << "-----------------------------------------------------------\n";
	Print(ids_weights);
	std::cout << "\n";
	updater.UpdateLookup(ids_weights, lookup.data());
	std::cout << "weights: ";
	report::Weights(lookup);
	std::cout << "clumps: ";
	report::Clumps(lookup);
	std::cout << '\n';
	std::cout << "-----------------------------------------------------------\n"
	          << "  Fragmentation report (<segment_size>x<count>) per real\n"
	          << "-----------------------------------------------------------\n";

	return 0;
}
