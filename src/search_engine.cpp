/// @file   search_engine.cpp
/// @brief  This file contains definition of SearchEngine class.
/// @author griha

#include "search_engine.h"

#include <iostream>

#include <boost/bind.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/options.hpp>
#include <boost/container/map.hpp>

#include <cryptopp/md5.h>
#include <cryptopp/sha.h>

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
        fs::path file_to_compare;
    };

    explicit Impl(SearchEngine::InitParams init_params)
        : algo(init_params.algo)
        , block_size(init_params.block_size)
        , file_min_size(init_params.file_min_size)
        , paths_scan(std::move(init_params.paths_scan))
        , paths_exclude(std::move(init_params.paths_exclude))
        , rxpatterns(std::move(init_params.rxpatterns)) {}

    hash_algo algo;
    size_t block_size;
    size_t file_min_size;
    SearchEngine::paths_type paths_scan;
    SearchEngine::paths_type paths_exclude;
    SearchEngine::rxpatterns_type rxpatterns;

    bool clean;
    Node root;

    void clear();
    void pre_process(const fs::path& file_path);
    void process(const fs::path& file_path);
    void run(bool recursive);
};

void SearchEngine::Impl::clear() {
    root.duplicates.clear();
    root.childs.clear();
    root.file_to_compare.clear();
    clean = true; 
}

void SearchEngine::Impl::pre_process(const fs::path& file_path) {
    if (!fs::is_regular_file(file_path))
        return;

    process(file_path);
}

void SearchEngine::Impl::process(const fs::path& file_path) {
    if (!match_any(file_path, rxpatterns) ||
            fs::file_size(file_path) < file_min_size)
        return;

    if (clean) {
        root.file_to_compare = file_path;
        clean = false;
        return;
    }

    

    for (Node& n = root;;) {
        if (!n.file_to_compare.empty())
    }
}

void SearchEngine::Impl::run(bool recursive) {
    clear();

    for (const auto& path : paths_scan) {
        if (!fs::exists(path)) {
            std::cerr << path << " is not exist" << std::endl;
            continue;
        }

        if (fs::is_regular_file(path)) {
            process(path);
            continue;
        }

        if (!fs::is_directory(path)) {
            std::cerr << path << " is not a regular or directory file" << std::endl;
            continue;
        }

        if (is_excluded(path, paths_exclude))
            continue;

        if (recursive)
            std::for_each(
                fs::directory_iterator{path}, fs::directory_iterator{},
                boost::bind(&Impl::pre_process, this, boost::placeholders::_1));
        else
            std::for_each(
                fs::recursive_directory_iterator{path}, fs::recursive_directory_iterator{},
                boost::bind(&Impl::pre_process, this, boost::placeholders::_1));
    }
}

SearchEngine::SearchEngine(InitParams init_params)
    : pimpl_(new Impl { std::move(init_params) }) {}

void SearchEngine::run(bool recursive) {
    pimpl_->run(recursive);
}

} // namespace griha