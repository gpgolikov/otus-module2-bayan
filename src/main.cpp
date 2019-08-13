#include <iostream>
#include <clocale>
#include <string>
#include <algorithm>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>

#include "search_engine.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace std::string_literals;

namespace griha {

/// @name Converters of custom types have to be supported by @c boost::program_options
/// @{

inline std::ostream& operator<< (std::ostream& os, hash_algo hash) {
    switch (hash) {
    case hash_algo::md5: os << "md5"; break;
    case hash_algo::sha256: os << "sha256"; break;
    default:
        throw po::invalid_option_value{ "expected: md5|sha256" };
    }
    return os;
}

inline std::istream& operator>> (std::istream& is, hash_algo& hash) {
    std::string value;
    is >> value;

    if (value == "md5"s)
        hash = hash_algo::md5;
    else if (value == "sha256"s)
        hash = hash_algo::sha256;
    else
        throw po::invalid_option_value{ "expected: md5|sha256" };
    return is;
}

/// @}

namespace {

/// @name Free functions to process command line options
/// @{

void usage(const char* argv0, std::ostream& os, const po::options_description& opts_desc) {
    os << "Usage:" << std::endl
       << '\t' << fs::path{ argv0 }.stem().c_str() << " [options] [<path-to-scan> ...]" << std::endl
       << '\t' << opts_desc << std::endl;
}

auto create_rxpatters(const std::wstring& patterns) {
    using separator_type = boost::char_separator<wchar_t>;
    using tokenizer_type = boost::tokenizer<separator_type, std::wstring::const_iterator, std::wstring>;

    std::vector<boost::wregex> ret;
    
    tokenizer_type tok { patterns, separator_type { L",;:" } };
    for (const auto& t : tok)
        ret.emplace_back(t, boost::regex::basic|boost::regex::icase);

    ret.shrink_to_fit();
    return ret;
}

/// @}

} // unnamed namespace
} // namespace griha

int main(int argc, char* argv[]) {
    using namespace griha;

    constexpr auto c_default_block_size = 1024;
    constexpr auto c_default_file_min_size = 1;
    constexpr auto c_default_hash_algo = griha::hash_algo::md5;

    bool opt_help, recursive;
    std::string patterns;
    std::vector<fs::path> paths_scan, paths_exclude;
    size_t file_min_size, block_size;
    hash_algo halgo;

    // command line options
    po::options_description generic { "Options" };
    generic.add_options()
            ("help,h", po::bool_switch(&opt_help), "prints out this message")
            ("exclude-path,E", po::value(&paths_exclude), "path to be excluded from scanning")
            ("patterns,P", po::value(&patterns), "patterns of files to be scanned")
            ("block-size,B", po::value(&block_size)->default_value(c_default_block_size),
                             "block size in bytes")
            ("min-size,S", po::value(&file_min_size)->default_value(c_default_file_min_size),
                           "minimum file size to be scanned in bytes")
            ("hash,H", po::value(&halgo)->default_value(c_default_hash_algo),
                       "hash algorithm, md5, sha256")
            ("recursive,r", po::value(&recursive), "scan recursively");

    // Next options allowed at command line, but isn't shown in help
    po::options_description hidden {};
    hidden.add_options()("scan-path", po::value(&paths_scan));
    po::positional_options_description pos;
    pos.add("scan-path", -1);

    po::options_description cmd_line, visible;
    cmd_line.add(generic).add(hidden);
    visible.add(generic);

    po::variables_map opts;
    po::store(po::command_line_parser(argc, argv).options(cmd_line).positional(pos).run(), opts);

    try {
        notify(opts);
    } catch (...) {
        usage(argv[0], std::cerr, visible);
        return EXIT_FAILURE;
    }

    if (opt_help) {
        usage(argv[0], std::cout, visible);
        return EXIT_SUCCESS;
    }

    if (paths_scan.empty())
        paths_scan.push_back(fs::current_path());

    for (auto& path : paths_exclude)
        path = fs::system_complete(path);

    SearchEngine::InitParams init_params = {
        halgo,
        block_size,
        file_min_size,
        std::move(paths_scan),
        std::move(paths_exclude),
        create_rxpatters(boost::from_local_8_bit(patterns))
    };
    SearchEngine sengine { std::move(init_params) };

    sengine.run(recursive);

    return EXIT_SUCCESS;
}