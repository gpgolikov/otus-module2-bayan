/// @file   search_engine.cpp
/// @brief  This file contains definition of SearchEngine class.
/// @author griha

#include "search_engine.h"

#include <iostream>

#include <boost/bind.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/options.hpp>
#include <boost/container/map.hpp>

namespace fs = boost::filesystem;
namespace cont = boost::container;

namespace griha {

namespace {

bool is_excluded(const fs::path& path, SearchEngine::paths_type& excl_paths) {
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
void apply_on_regular_files(DirIt f, DirIt l, SearchEngine::paths_type& excl_paths, Func&& fn) {
    for (; f != l; ++f) {
        const auto& dir_entry = *f;
        if (is_excluded(dir_entry.path(), excl_paths) ||
            !fs::is_regular_file(dir_entry.path()))
            continue;
        fn(dir_entry.path());
    }
}

bool match_any(const fs::path& p, const SearchEngine::rxpatterns_type& patterns) {
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

struct SearchEngine::Impl : boost::intrusive_ref_counter<SearchEngine::Impl, boost::thread_unsafe_counter> {

    struct Node {
        using vector_options = cont::vector_options_t<cont::growth_factor<cont::growth_factor_50>>;

        cont::vector<fs::path, void, vector_options> duplicates;
        cont::map<std::string, Node> childs;
    };

    explicit Impl(SearchEngine::InitParams init_params)
        : paths_scan_(std::move(init_params.paths_scan))
        , paths_exclude_(std::move(init_params.paths_exclude))
        , rxpatterns_(std::move(init_params.rxpatterns)) {}

    SearchEngine::paths_type paths_scan_;
    SearchEngine::paths_type paths_exclude_;
    SearchEngine::rxpatterns_type rxpatterns_;

    void run(bool recurdive);

    void process(const boost::filesystem::path& file_path);
};

void SearchEngine::Impl::process(const boost::filesystem::path& file_path)
{

}

void SearchEngine::Impl::run(bool recursive) {
    for (const auto& path : paths_scan_) {
        if (!fs::exists(path)) {
            std::cerr << path << " is not exist" << std::endl;
            continue;
        }

        if (fs::is_regular_file(path) && match_any(path, rxpatterns_)) {
            process(path);
            continue;
        }

        if (!fs::is_directory(path)) {
            std::cerr << path << " is not a regular or directory file" << std::endl;
            continue;
        }

        if (is_excluded(path, paths_exclude_))
            continue;

        if (recursive)
            std::for_each(
                fs::directory_iterator{path}, fs::directory_iterator{},
                boost::bind(&Impl::process, this, boost::placeholders::_1));
        else
            std::for_each(
                fs::recursive_directory_iterator{path}, fs::recursive_directory_iterator{},
                boost::bind(&Impl::process, this, boost::placeholders::_1));
    }
}

SearchEngine::SearchEngine(InitParams init_params)
    : pimpl_(new Impl { std::move(init_params) }) {}

void SearchEngine::run(bool recursive) {
    pimpl_->run(recursive);
}

} // namespace griha