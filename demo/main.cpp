#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>

#include "chash.hpp"
#include "report.hpp"

std::tuple<std::vector<std::string>,
           std::vector<std::uint32_t>,
           std::vector<std::uint32_t>>
ReadConfig(std::istream& config)
{

	std::tuple<std::vector<std::string>,
	           std::vector<std::uint32_t>,
	           std::vector<std::uint32_t>>
	        result;
	auto& [reals, ids, weights] = result;

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
		reals.push_back(real);
		ids.push_back(id);
		weights.push_back(weight);
	}
	return result;
}

std::tuple<std::vector<std::string>,
           std::vector<std::uint32_t>,
           std::vector<std::uint32_t>>
ReadConfigFile(std::string path)
{
	std::ifstream config(path);
	if (!config.is_open())
	{
		std::cout << "Failed to open config file.\n";
		std::exit(EXIT_FAILURE);
	}

	return ReadConfig(config);
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

using namespace std::string_view_literals;

static constexpr std::string_view FLAG_STDIN = "--stdin"sv;
static constexpr std::string_view FLAG_CONFIG = "--config"sv;
static constexpr std::string_view FLAG_CONFIG_SHORT = "-C"sv;
static constexpr std::string_view FLAG_CELLS_PER_WEIGHT = "--cells"sv;
static constexpr std::string_view FLAG_CELLS_PER_WEIGHT_SHORT = "-c"sv;
static constexpr std::string_view FLAG_MAPPINGS = "--mappings"sv;
static constexpr std::string_view FLAG_MAPPINGS_SHORT = "-m"sv;

static constexpr std::string_view CMD_REPORT_MAXERROR = "maxerror";
static constexpr std::string_view CMD_REPORT_MAXERROR_SERIES = "maxerrorseries";

static constexpr std::size_t DEFAULT_CELLS_PER_WEIGHT = 20;
static constexpr std::size_t DEFAULT_MAPPINGS = 20000;

enum class MainArg
{
	CONFIG,
	CELLS,
	MAPPINGS,
	STDIN,
	UNKNOWN
};

enum class ConfigSource
{
	IMAGINATION,
	FILE,
	STDIN
};

enum class Command
{
	MAXERROR,
	MAXERRORSERIES,
	NONE
};

MainArg ParseArg(const char* str)
{
	if (str == FLAG_CONFIG_SHORT || str == FLAG_CONFIG)
	{
		return MainArg::CONFIG;
	}

	if (str == FLAG_CELLS_PER_WEIGHT_SHORT || str == FLAG_CELLS_PER_WEIGHT)
	{
		return MainArg::CELLS;
	}

	if (str == FLAG_MAPPINGS_SHORT || str == FLAG_MAPPINGS)
	{
		return MainArg::MAPPINGS;
	}

	if (str == FLAG_STDIN)
	{
		return MainArg::STDIN;
	}

	return MainArg::UNKNOWN;
}

std::optional<std::size_t> ParseUint64(const std::string& s)
{
	try
	{
		return std::stoull(s);
	}
	catch (std::out_of_range& e)
	{
		std::cout << "Unsigned integer out of range\n";
		return std::nullopt;
	}
	catch (std::invalid_argument& e)
	{
		std::cout << "Invalid representation of unsigned integer\n";
		return std::nullopt;
	}
}

std::optional<Command> ParseCmd(const char* str)
{
	if (str == CMD_REPORT_MAXERROR)
	{
		return Command::MAXERROR;
	}
	if (str == CMD_REPORT_MAXERROR_SERIES)
	{
		return Command::MAXERRORSERIES;
	}

	return std::nullopt;
}

std::unordered_map<std::uint32_t, std::size_t> CellCount(const std::vector<std::uint32_t>& lookup)
{
	std::unordered_map<std::uint32_t, std::size_t> cells;
	for (const auto& id : lookup)
	{
		++cells[id];
	}
	return cells;
}

double MaxError(const std::vector<std::uint32_t>& ids,
                const std::vector<std::uint32_t>& weights,
                const std::vector<std::uint32_t>& lookup)
{
	auto dist = CellCount(lookup);
	std::size_t requesttotal = std::accumulate(weights.cbegin(),
	                                           weights.cend(),
	                                           std::size_t{});
	double emax{};
	for (std::size_t i = 0; i < ids.size(); ++i)
	{
		auto& id = ids[i];
		auto& rweight = weights[i];
		double requested = double(rweight) / requesttotal;
		double got = double(dist[id]) / lookup.size();
		double delta = std::abs(got - requested);
		emax = std::max(emax, delta / requested);
	}
	return emax;
}

class IpV6Address
{
	std::array<std::uint16_t, 8> data_;

public:
	IpV6Address() = default;
	IpV6Address(const IpV6Address& other) :
	        data_ { other.data_ }
	{
	}
	static constexpr std::string_view GAP_TOKEN = "::";
	static constexpr std::string_view::value_type DELIM_TOKEN = ':';
	static std::optional<IpV6Address> FromString(std::string_view sw)
	{
		IpV6Address ip;
		auto& data = ip.data_;

		auto gap = sw.find(GAP_TOKEN);
		std::string_view left = sw.substr(0, gap);

		std::size_t i = 0;
		// Parse left
		for (; i < data.size(); ++i)
		{
			auto [ptr, ec] = std::from_chars(left.data(), left.data() + left.size(), data.at(i), 16);

			if (ec == std::errc::invalid_argument)
			{
				return std::nullopt;
			}

			left.remove_prefix(std::distance(left.data(), ptr));
			if (!left.empty())
			{
				if (left.front() == DELIM_TOKEN)
				{
					left.remove_prefix(1);
				}
				else
				{
					return std::nullopt;
				}
			}
			else
			{
				++i;
				break;
			}
		}

		if (gap == std::string::npos)
		{
			if (i == data.size())
			{
				return ip;
			}
			else
			{
				return std::nullopt;
			}
		}

		std::string_view right = sw.substr(gap + 2);

		// Gap
		auto rlen = std::count(right.begin(), right.end(), DELIM_TOKEN);
		if (i + rlen > data.size())
		{
			return std::nullopt;
		}
		for (auto gend = data.size() - rlen; i < gend; ++i)
		{
			data.at(i) = 0;
		}

		// Parse right
		for (; i < data.size(); ++i)
		{
			auto [ptr, ec] = std::from_chars(right.data(), right.data() + right.size(), data.at(i), 16);

			if (ec == std::errc::invalid_argument)
			{
				return std::nullopt;
			}

			right.remove_prefix(std::distance(right.data(), ptr));
			if (!right.empty())
			{
				if (right.front() == DELIM_TOKEN)
				{
					right.remove_prefix(1);
				}
				else
				{
					return std::nullopt;
				}
			}
			else
			{
				break;
			}
		}

		return ip;
	}

	std::array<std::uint16_t, 8>& Data()
	{
		return data_;
	}

	std::size_t Hash() const
	{
		const std::uint64_t* d = reinterpret_cast<const std::uint64_t*>(data_.data());
		return d[0] ^ (d[1] << 1);
	}

	bool operator<(const IpV6Address& other) const
	{
		for (std::size_t i = 0; i < data_.size(); ++i)
		{
			if (data_[i] != other.data_[i])
			{
				return data_[i] < other.data_[i];
			}
		}
		return false;
	}

	bool operator==(const IpV6Address& other) const
	{
		for (std::size_t i = 0; i < data_.size(); ++i)
		{
			if (data_[i] != other.data_[i])
			{
				return false;
			}
		}
		return true;
	}
};

template<>
struct std::hash<IpV6Address>
{
	std::size_t operator()(const IpV6Address& s) const noexcept
	{
		return s.Hash();
	}
};

class IpV6Gen
{
	std::mt19937_64 rg_;
	std::vector<IpV6Address> provided_;
	std::unordered_set<IpV6Address> used_;

public:
	IpV6Gen(const std::set<IpV6Address>& predefined) :
	        rg_{std::random_device{}()},
	        provided_{predefined.begin(), predefined.end()}
	{
		std::shuffle(provided_.begin(), provided_.end(), std::mt19937(std::random_device{}()));
	}

	IpV6Address Unique()
	{
		IpV6Address ip;
		if (!provided_.empty())
		{
			ip = provided_.back();
			provided_.pop_back();
			used_.insert(ip);
		}
		else
		{
			ip = Random();
			while (!used_.insert(ip).second)
			{
				ip = Random();
			}
		}
		return ip;
	}

	IpV6Address Random()
	{
		IpV6Address addr;
		std::uint64_t* d = reinterpret_cast<std::uint64_t*>(addr.Data().data());
		d[0] = rg_();
		d[1] = rg_();
		return addr;
	}
};

std::set<IpV6Address> ParseIpV6Set(std::istream& iplist)
{
	std::set<IpV6Address> ipset;
	std::string s;
	while (iplist)
	{
		iplist >> s;
		auto mbip = IpV6Address::FromString(s);
		if (mbip)
		{
			ipset.insert(mbip.value());
		}
		else
		{
			std::cerr << "Expected IpV6Address, got '" << s << "'\n";
		}
	}
	return ipset;
}

void MaxErrorSeries(std::size_t step, std::size_t limit, std::size_t mappings, std::size_t cells, const std::set<IpV6Address>& ipset)
{
	std::vector<double> error;

	IpV6Gen gen(ipset);
	std::vector<std::pair<IpV6Address, std::uint32_t>> real_ids = {{gen.Unique(), 1}};
	std::vector<std::uint32_t> ids{1};
	std::vector<std::uint32_t> weights{1};

	while (real_ids.size() < limit)
	{
		std::cerr << real_ids.size() << "\n";
		auto oupdater = chash::MakeWeightUpdater(real_ids, mappings, cells);

		if (!oupdater)
		{
			std::cerr << "Failed to build updater\n";
			std::exit(EXIT_FAILURE);
		}
		auto& updater = oupdater.value();

		std::vector<std::uint32_t> lookup(updater.LookupSize(), std::numeric_limits<std::uint32_t>::max());

		updater.InitLookup(lookup.data());

		error.push_back(MaxError(ids, weights, lookup));

		for (std::size_t i = 0; i < step; ++i)
		{
			real_ids.emplace_back(gen.Unique(), real_ids.size() + 1);
			ids.push_back(real_ids.size());
			weights.push_back(100);
		}
	}
}

std::set<IpV6Address> ReadIPSet()
{
	std::ifstream config("../reals.only");
	std::set<IpV6Address> ipset;
	if (!config.is_open())
	{
		std::cout << "Failed to open reals file.\n";
		std::exit(EXIT_FAILURE);
	}
	return ParseIpV6Set(config);
}

int main(int argc, char* argv[])
{
	ConfigSource config_source{ConfigSource::IMAGINATION};
	std::optional<std::string> config_path;
	std::size_t cells{DEFAULT_CELLS_PER_WEIGHT};
	std::size_t mappings{DEFAULT_MAPPINGS};
	int i = 1;
	for (; i < argc - 1; ++i)
	{
		switch (ParseArg(argv[i]))
		{
			case MainArg::CONFIG:
				if (config_source != ConfigSource::IMAGINATION)
				{
					std::cerr << "--config conflicts with previous flags\n";
					std::exit(EXIT_FAILURE);
				}
				config_source = ConfigSource::FILE;
				++i;
				if (i >= argc)
				{
					std::cerr << "--config requires path argument\n";
					std::exit(EXIT_FAILURE);
				}
				config_path = argv[i];
				break;
			case MainArg::STDIN:
				if (config_source != ConfigSource::IMAGINATION)
				{
					std::cerr << "--config conflicts with previous flags\n";
					std::exit(EXIT_FAILURE);
				}
				config_source = ConfigSource::STDIN;
				break;
			case MainArg::CELLS:
				++i;
				if (i >= argc)
				{
					std::cerr << "--cells requires unsigned integer argument\n";
					std::exit(EXIT_FAILURE);
				}
				if (auto cellarg = ParseUint64(std::string{argv[i]}))
				{
					cells = cellarg.value();
				}
				else
				{
					std::cerr << "invalid value for --cells\n";
					std::exit(EXIT_FAILURE);
				}
				break;
			case MainArg::MAPPINGS:
				++i;
				if (i >= argc)
				{
					std::cerr << "--mappings requires unsigned integer argument\n";
					std::exit(EXIT_FAILURE);
				}
				if (auto maparg = ParseUint64(std::string{argv[i]}))
				{
					mappings = maparg.value();
				}
				else
				{
					std::cerr << "invalid value for --mappings\n";
					std::exit(EXIT_FAILURE);
				}
				mappings = std::stoull(std::string(argv[i]));
				break;
			case MainArg::UNKNOWN:
				std::cerr << "Unknown argument " << i << " '" << argv[i] << "'\n";
				std::exit(EXIT_FAILURE);
				break;
		};
	}
	Command cmd{};
	if (i == argc - 1)
	{
		if (auto c = ParseCmd(argv[i]))
		{
			cmd = c.value();
		}
		else
		{
			std::cerr << "Unrecognized command '" << argv[i] << "'\n";
			std::exit(EXIT_FAILURE);
		}
	}

	std::vector<std::string> reals;
	std::vector<std::uint32_t> ids;
	std::vector<std::uint32_t> weights;

	switch (config_source)
	{
		case ConfigSource::FILE:
			std::tie(reals, ids, weights) = ReadConfigFile(config_path.value());
			break;
		case ConfigSource::STDIN:
			std::tie(reals, ids, weights) = ReadConfig(std::cin);
			break;
		case ConfigSource::IMAGINATION:
			reals = {"alpha", "beta", "gamma", "delta", "epsilon", "sigma", "theta", "omega", "more"};
			ids = {0, 1, 2, 3, 4, 5, 6, 7, 8};
			weights = {100, 100, 50, 10, 1, 100, 100, 100, 100};
			break;
	}

	std::vector<std::pair<std::string, std::uint32_t>> real_ids;
	for (std::size_t i = 0; i < reals.size(); ++i)
	{
		real_ids.emplace_back(reals.at(i), ids.at(i));
		// std::cout << reals.at(i) << " " << ids.at(i) << weights.at(i) << "\n";
	}

	auto oupdater = chash::MakeWeightUpdater(real_ids, mappings, cells);

	if (!oupdater)
	{
		std::cerr << "Failed to build updater\n";
		std::exit(EXIT_FAILURE);
	}
	auto& updater = oupdater.value();

	std::vector<std::uint32_t> lookup(updater.LookupSize(), std::numeric_limits<std::uint32_t>::max());

	updater.InitLookup(lookup.data());

	switch (cmd)
	{
		case Command::MAXERRORSERIES:
		{
			auto ipset = ReadIPSet();
			MaxErrorSeries(10, 300, 100, 20, ipset);
		}
		break;
		case Command::MAXERROR:
			std::cout << MaxError(ids, weights, lookup);
			break;
		default:
		{
			std::cout << "-----------------------------------------------------------\n"
			          << "  Reals requested\n"
			          << "-----------------------------------------------------------\n";
			Print(real_ids);
			std::cout << '\n';

			std::cout << "\n"
			             "-----------------------------------------------------------\n"
			             "  Init statistics\n"
			             "-----------------------------------------------------------\n";

			std::vector<std::pair<std::uint32_t, std::uint32_t>> ids_weights;
			for (std::size_t i = 0; i < reals.size(); ++i)
			{
				ids_weights.emplace_back(ids.at(i), weights.at(i));
			}

			// std::cout << "Weight sum is " << WeightSum(ids_weights) << "\n";

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
		}
		break;
	}

	return 0;
}
