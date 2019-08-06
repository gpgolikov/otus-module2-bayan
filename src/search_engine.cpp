/// @file   search_engine.cpp
/// @brief  This file contains definition of SearchEngine class.
/// @author griha

#include "search_engine.h"

namespace griha {

namespace {

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

bool match_any(const fs::path& p, const std::vector<boost::wregex>& patterns) {
    if (patterns.empty())
        return true;

    for (const auto& pattern : patterns) {
        if (!boost::regex_match(p.filename().wstring(), pattern))
            continue;
        return true;
    }
    return false;
}

} // unnamed namespace

SearchEngine::SearchEngine(InitParams init_params)
    : paths_scan_(std::move(init_params.paths_scan))
    , paths_exclude_(std::move(init_params.paths_exclude))
    , rxpatterns_(std::move(init_params.rxpatterns)) {}

void SearchEngine::run() {
    for (auto& path : paths_scan_) {
        if (!fs::exists(path)) {
            std::cerr << "file " << path << " is not exist";
            usage(argv[0], std::cerr, visible);
            return EXIT_FAILURE;
        }

        if (fs::is_regular_file(scan_path) && match_any(scan_path, rxpatterns)) {
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

}

} // namespace griha