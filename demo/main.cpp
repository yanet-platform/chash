#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

#include "chash.hpp"
#include "printers.hpp"
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
	auto print = [](std::ostream& o, decltype(hid)& i) {
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
static constexpr std::string_view CMD_REPORT_MISSING = "missing";
static constexpr std::string_view CMD_REPORT_OVERLAP = "overlap";
static constexpr std::string_view CMD_REPORT_TIME = "time";
static constexpr std::string_view CMD_REPORT_YIELD_UNIFORMITY_ABS = "yielduniabs";
static constexpr std::string_view CMD_REPORT_YIELD_UNIFORMITY_ABS_MAX = "maxyielduniabs";

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
	NONE,
	MAXERROR,
	MAXERRORSERIES,
	MISSING,
	OVERLAP,
	TIME,
	YIELD_UNIFORMITY_ABS,
	YIELD_UNIFORMITY_ABS_MAX
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
	if (str == CMD_REPORT_MISSING)
	{
		return Command::MISSING;
	}
	if (str == CMD_REPORT_OVERLAP)
	{
		return Command::OVERLAP;
	}
	if (str == CMD_REPORT_TIME)
	{
		return Command::TIME;
	}
	if (str == CMD_REPORT_YIELD_UNIFORMITY_ABS)
	{
		return Command::YIELD_UNIFORMITY_ABS;
	}
	if (str == CMD_REPORT_YIELD_UNIFORMITY_ABS_MAX)
	{
		return Command::YIELD_UNIFORMITY_ABS_MAX;
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
		if (double n = delta / requested; std::abs(n) > std::abs(emax))
		{
			emax = n;
		}
	}
	return emax;
}

class IpV6Address
{
	std::array<std::uint16_t, 8> data_;

public:
	static constexpr std::string_view GAP_TOKEN = "::";
	static constexpr std::string_view::value_type DELIM_TOKEN = ':';
	static std::optional<IpV6Address> FromString(const std::string ips)
	{
		IpV6Address ip;
		if (inet_pton(AF_INET6, ips.c_str(), ip.data_.data()) != 1)
		{
			return std::nullopt;
		}
		return ip;
	}

	std::string ToString() const
	{
		char buf[INET_ADDRSTRLEN];
		if (!inet_ntop(AF_INET6, data_.data(), buf, INET6_ADDRSTRLEN))
		{
			throw std::logic_error{"ntop conversion failed"};
		}
		return buf;
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
	std::vector<IpV6Address> reals{gen.Unique()};
	std::vector<std::uint32_t> ids{1};
	std::vector<std::uint32_t> weights{1};

	while (reals.size() < limit)
	{
		auto oupdater = chash::MakeWeightUpdater(
		        reals.data(),
		        ids.data(),
		        weights.data(),
		        reals.size(),
		        mappings,
		        cells);

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
			reals.emplace_back(gen.Unique());
			ids.push_back(reals.size());
			weights.push_back(100);
		}
	}
}

std::optional<std::set<IpV6Address>> ReadIPSet()
{
	std::ifstream config("../reals.only");
	std::set<IpV6Address> ipset;
	if (!config.is_open())
	{
		std::exit(EXIT_FAILURE);
	}
	return ParseIpV6Set(config);
}
using lookup_t = std::vector<std::uint32_t>;

double diff(const lookup_t& a, const lookup_t& b)
{
	if (a.size() != b.size())
	{
		throw std::invalid_argument{"Mismatched lookups size"};
	}

	std::size_t mismatch{};
	for (std::size_t i = 0; i < a.size(); ++i)
	{
		if (a[i] != b[i])
		{
			++mismatch;
		}
	}

	return static_cast<double>(mismatch) / a.size();
}

double same(const lookup_t& a, const lookup_t& b)
{
	if (a.size() != b.size())
	{
		throw std::invalid_argument{"Mismatched lookups size"};
	}

	std::size_t match{};
	for (std::size_t i = 0; i < a.size(); ++i)
	{
		if (a[i] == b[i])
		{
			++match;
		}
	}

	return static_cast<double>(match) / a.size();
}

std::unordered_map<std::uint32_t, std::uint32_t> dist(const lookup_t& a, const lookup_t& b)
{
	if (a.size() != b.size())
	{
		throw std::invalid_argument{"Mismatched lookups size"};
	}

	std::unordered_map<std::uint32_t, std::uint32_t> result;
	for (std::size_t i = 0; i < a.size(); ++i)
	{
		if (a[i] != b[i])
		{
			++result[b[i]];
		}
	}

	return result;
}

void Overlap(std::set<IpV6Address>& ipset, std::uint32_t mappings, std::uint32_t cells)
{
	std::size_t cnt = ipset.size();
	const std::size_t percent = cnt / 100;
	std::vector<IpV6Address> aset{ipset.begin(), ipset.end()};
	std::vector<IpV6Address> bset{ipset.rbegin(), ipset.rend()};
	std::vector<std::uint32_t> aids(cnt, 0);
	std::iota(aids.begin(), aids.end(), 1);
	std::vector<std::uint32_t> bids(cnt, 0);
	std::iota(bids.rbegin(), bids.rend(), 1);
	std::vector<std::uint32_t> weights(cnt, 100);

	const auto& sz = chash::WeightUpdater::LookupRequiredSize(cnt, cells);
	std::vector<std::uint32_t> alook(sz, 0);
	std::vector<std::uint32_t> blook(sz, 0);

	for (std::uint8_t i = 0; i < 50; ++i)
	{
		auto apdater = chash::MakeWeightUpdater(
		        aset.data(), aids.data(), weights.data(), aset.size(), mappings, cells, sz);
		auto bpdater = chash::MakeWeightUpdater(
		        bset.data(), bids.data(), weights.data(), bset.size(), mappings, cells, sz);

		if (!apdater || !bpdater)
		{
			std::cerr << "Failed to create updater" << std::endl;
			std::exit(EXIT_FAILURE);
		}

		std::fill(alook.begin(), alook.end(), std::numeric_limits<std::uint32_t>::max());
		std::fill(blook.begin(), blook.end(), std::numeric_limits<std::uint32_t>::max() - 1);

		apdater.value().InitLookup(alook.data());
		bpdater.value().InitLookup(blook.data());
		std::cout << diff(alook, blook) << '\n';

		for (std::size_t j = 0; j < percent; ++j)
		{
			if (aset.empty())
			{
				break;
			}
			aset.pop_back();
			bset.pop_back();
		}
	}
}

auto PrepareUpdater(std::set<IpV6Address>& ipset, std::uint32_t mappings, std::uint32_t cells, std::uint8_t weight)
{
	std::size_t cnt = ipset.size();
	std::vector<IpV6Address> aset{ipset.begin(), ipset.end()};
	std::mt19937 gen(42);
	std::shuffle(aset.begin(), aset.end(), gen);
	std::vector<std::uint32_t> ids(cnt, 0);
	std::iota(ids.begin(), ids.end(), 1);
	std::vector<std::uint32_t> weights(cnt, weight);
	const auto& sz = chash::WeightUpdater::LookupRequiredSize(cnt, cells);
	auto oapdater = chash::MakeWeightUpdater(
	        aset.data(), ids.data(), weights.data(), aset.size(), mappings, cells, sz);
	if (!oapdater)
	{
		throw std::runtime_error{"Failed to create updater"};
	}
	return oapdater.value();
}

void Difference(std::set<IpV6Address>& ipset, std::uint32_t mappings, std::uint32_t cells)
{
	const auto& sz = chash::WeightUpdater::LookupRequiredSize(ipset.size(), cells);
	std::vector<std::uint32_t> alook(sz, 0);
	std::vector<std::uint32_t> blook(sz, 0);
	std::fill(alook.begin(), alook.end(), std::numeric_limits<std::uint32_t>::max());
	std::fill(blook.begin(), blook.end(), std::numeric_limits<std::uint32_t>::max() - 1);

	auto updater = PrepareUpdater(ipset, mappings, cells, 100);

	updater.InitLookup(alook.data());
	updater.InitLookup(blook.data());

	std::vector<std::uint32_t> disable_order(ipset.size(), 0);
	std::iota(disable_order.begin(), disable_order.end(), 1);
	std::mt19937 gen(42);
	std::shuffle(disable_order.begin(), disable_order.end(), gen);

	std::cout << "disfrac;similarity\n";
	for (std::size_t i = 0; i < disable_order.size() - 1; ++i)
	{
		updater.UpdateWeight(disable_order.at(i), 0, blook.data());
		std::cout << double(i + 1) / disable_order.size() << ";" << same(alook, blook) << '\n';
	}
}

void Time(std::set<IpV6Address>& ipset, std::uint32_t mappings, std::uint32_t cells)
{
	const auto& sz = chash::WeightUpdater::LookupRequiredSize(ipset.size(), cells);
	std::vector<std::uint32_t> alook(sz, 0);
	std::fill(alook.begin(), alook.end(), std::numeric_limits<std::uint32_t>::max());

	auto start = std::chrono::steady_clock::now();
	auto updater = PrepareUpdater(ipset, mappings, cells, 100);
	auto init = std::chrono::steady_clock::now();
	updater.InitLookup(alook.data());
	auto end = std::chrono::steady_clock::now();
	std::cout << std::chrono::duration_cast<std::chrono::duration<double>>(init - start).count() << '\n'
	          << std::chrono::duration_cast<std::chrono::duration<double>>(end - init).count() << '\n';
}

void DifferenceUniformityAbsolute(std::set<IpV6Address>& ipset, std::uint32_t mappings, std::uint32_t cells)
{
	std::vector<std::uint32_t> ids(ipset.size(), 0);
	std::iota(ids.begin(), ids.end(), 1);
	std::vector<std::uint32_t> weights(ipset.size(), 100);

	const auto& sz = chash::WeightUpdater::LookupRequiredSize(ipset.size(), cells);
	std::vector<std::uint32_t> alook(sz, 0);
	std::vector<std::uint32_t> blook(sz, 0);

	std::fill(alook.begin(), alook.end(), std::numeric_limits<std::uint32_t>::max());

	auto updater = PrepareUpdater(ipset, mappings, cells, 100);

	updater.InitLookup(alook.data());
	updater.InitLookup(blook.data());
	std::size_t norm = alook.size() / ipset.size();

	auto d = dist(alook, blook);
	auto printrow = [&](std::size_t iteration, double norm) {
		auto d = dist(alook, blook);
		std::uint32_t md{};
		std::uint32_t mit{};
		std::size_t mid{};
		for (std::size_t i = 0; i < ids.size(); ++i)
		{
			if (d.find(i) != d.end())
			{
				if (d[i] >= md)
				{
					md = d[i];
					mit = iteration;
					mid = i;
				}
				if (i % 10 == 0)
				{
					std::cout << mit << ";"
					          << mid << ";"
					          << (md / norm) << '\n';
					md = 0;
				}
			}
		}
	};
	printrow(0, norm);

	for (std::size_t i = 1; i < ids.size(); ++i)
	{
		updater.UpdateWeight(ids.at(ids.size() - i), 0, blook.data());
		printrow(i, double(alook.size()) / (ids.size() - i));
	}
}

void DifferenceUniformityAbsoluteMax(std::set<IpV6Address>& ipset, std::uint32_t mappings, std::uint32_t cells)
{
	std::vector<std::uint32_t> ids(ipset.size(), 0);
	std::iota(ids.begin(), ids.end(), 1);
	std::vector<std::uint32_t> weights(ipset.size(), 100);

	const auto& sz = chash::WeightUpdater::LookupRequiredSize(ipset.size(), cells);
	std::vector<std::uint32_t> alook(sz, 0);
	std::vector<std::uint32_t> blook(sz, 0);

	std::fill(alook.begin(), alook.end(), std::numeric_limits<std::uint32_t>::max());

	auto updater = PrepareUpdater(ipset, mappings, cells, 100);

	updater.InitLookup(alook.data());
	updater.InitLookup(blook.data());
	std::size_t norm = alook.size() / ipset.size();

	auto d = dist(alook, blook);
	auto printrow = [&](double norm) {
		auto d = dist(alook, blook);
		std::uint32_t md{};
		for (std::size_t i = 0; i < ids.size(); ++i)
		{
			if (d.find(i) != d.end())
			{
				if (d[i] >= md)
				{
					md = d[i];
				}
			}
		}
		std::cout << (md / norm) << '\n';
	};
	printrow(norm);

	for (std::size_t i = 1; i < ids.size(); ++i)
	{
		updater.UpdateWeight(ids.at(ids.size() - i), 0, blook.data());
		printrow(double(alook.size()) / (ids.size() - i));
	}
}

void EffectiveWeightsByWeight(std::set<IpV6Address>& ipset, std::uint32_t mappings, std::uint32_t cells)
{
	std::vector<std::uint32_t> ids(ipset.size(), 0);
	std::iota(ids.begin(), ids.end(), 1);

	const auto& sz = chash::WeightUpdater::LookupRequiredSize(ipset.size(), cells);
	std::vector<std::uint32_t> alook(sz, 0);

	std::fill(alook.begin(), alook.end(), std::numeric_limits<std::uint32_t>::max());

	auto updater = PrepareUpdater(ipset, mappings, cells, 100);

	updater.InitLookup(alook.data());
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
			weights = {1, 1, 1, 1, 100, 1, 1, 1, 1};
			break;
	}

	std::vector<std::pair<std::string, std::uint32_t>> real_ids;
	for (std::size_t i = 0; i < reals.size(); ++i)
	{
		real_ids.emplace_back(reals.at(i), ids.at(i));
	}

	auto ipset = ReadIPSet();

	switch (cmd)
	{
		case Command::MAXERRORSERIES:
		{
			MaxErrorSeries(10, 300, 100, 20, ipset.value());
		}
		break;
		case Command::MAXERROR:
			break;
		case Command::OVERLAP:
			Overlap(ipset.value(), mappings, cells);
			break;
		case Command::MISSING:
			Difference(ipset.value(), mappings, cells);
			break;
		case Command::TIME:
			Time(ipset.value(), mappings, cells);
			break;
		case Command::YIELD_UNIFORMITY_ABS:
			DifferenceUniformityAbsolute(ipset.value(), mappings, cells);
			break;
		case Command::YIELD_UNIFORMITY_ABS_MAX:
			DifferenceUniformityAbsoluteMax(ipset.value(), mappings, cells);
			break;
		default:
		{
			auto oupdater = chash::MakeWeightUpdater(reals.data(),
			                                         ids.data(),
			                                         weights.data(),
			                                         reals.size(),
			                                         mappings,
			                                         cells);

			if (!oupdater)
			{
				std::cerr << "Failed to build updater\n";
				std::exit(EXIT_FAILURE);
			}
			auto& updater = oupdater.value();

			std::vector<std::uint32_t> lookup(updater.LookupSize(), std::numeric_limits<std::uint32_t>::max());

			updater.InitLookup(lookup.data());

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
			updater.UpdateLookup(ids.data(), weights.data(), ids.size(), lookup.data());
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
