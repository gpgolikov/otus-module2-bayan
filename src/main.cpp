#include <iostream>
#include <clocale>
#include <string>
#include <algorithm>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace std::string_literals;

namespace griha {

enum class hash_algo {
    md5,
    sha256
};

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

} // namespace griha

namespace {

void usage(const char* argv0, std::ostream& os, const po::options_description& opts_desc) {
    os << "Usage:" << std::endl
       << '\t' << fs::path{ argv0 }.stem().c_str() << " [options] [<path-to-scan> ...]" << std::endl
       << '\t' << opts_desc << std::endl;
}

bool is_excluded(const fs::path& path, std::vector<fs::path>& excl_paths) {
    if (excl_paths.empty())
        return false;

    const auto p = fs::system_complete(path);
    const auto it = std::find(excl_paths.begin(), excl_paths.end(), p);
    if (it == excl_paths.end())
        return false;

    excl_paths.erase(it);
    return true;
}

template <typename DirIt, typename Func>
void apply_on_regular_files(DirIt f, DirIt l, std::vector<fs::path>& excl_paths, Func&& fn) {
    for (; f != l; ++f) {
        const auto& dir_entry = *f;
        if (is_excluded(dir_entry.path(), excl_paths) ||
            !fs::is_regular_file(dir_entry.path()))
            continue;
        fn(dir_entry.path());
    }
}

bool match(const fs::path& p, const std::vector<boost::regex>& patterns) {
    if (patterns.empty())
        return true;

    for (const auto& pattern : patterns) {
        if (!boost::regex_match(p.filename().string(), pattern))
            continue;
        return true;
    }
    return false;
}

} // unnamed namespace

int main(int argc, char* argv[]) {
    constexpr auto c_default_block_size = 1024;
    constexpr auto c_default_file_min_size = 1;
    constexpr auto c_default_hash_algo = griha::hash_algo::md5;

    bool opt_help, recursive;
    std::string patterns;
    std::vector<fs::path> scan_paths, excl_paths;
    size_t file_min_size, block_size;
    griha::hash_algo halgo;

    // command line options
    po::options_description generic { "Options" };
    generic.add_options()
            ("help,h", po::bool_switch(&opt_help), "prints out this message")
            ("exclude-path,E", po::value(&excl_paths), "path to be excluded from scanning")
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
    hidden.add_options()("scan-path", po::value(&scan_paths));
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

    if (scan_paths.empty())
        scan_paths.push_back(fs::current_path());

    for (auto& excl_path : excl_paths)
        excl_path = fs::system_complete(excl_path);

    std::vector<boost::regex> patterns;
    
    boost::tokenizer tokenizer;

    for (auto& scan_path : scan_paths) {
        if (!fs::exists(scan_path)) {
            std::cerr << "file " << scan_path << " is not exist";
            usage(argv[0], std::cerr, visible);
            return EXIT_FAILURE;
        }

        if (fs::is_regular_file(scan_path) && match(scan_path, )) {
            files.push_back(scan_path);
            continue;
        }

        if (!fs::is_directory(scan_path)) {
            std::cerr << "file " << scan_path << " is not a regular file or directory";
            usage(argv[0], std::cerr, visible);
            return EXIT_FAILURE;
        }

        if (is_excluded(scan_path, excl_paths))
            continue;

        if (recursive)
            search_regular_files(
                fs::directory_iterator{p}, fs::directory_iterator{},
                std::back_inserter(files));
        else
            search_regular_files(
                fs::recursive_directory_iterator{p}, fs::recursive_directory_iterator{},
                std::back_inserter(files));
    }

    return EXIT_SUCCESS;
}