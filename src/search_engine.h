/// @file   search_engine.h
/// @brief  This file contains declaration of SearchEngine class provides base search
///         driver that is searching for duplicates of files in specified locations.
/// @author griha

#pragma once

#include <vector>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/intrusive_ptr.hpp>

namespace griha {

enum class hash_algo {
    md5,
    sha256
};

class SearchEngine {

    struct Impl;

public:

    using paths_type = std::vector<boost::filesystem::path>;
    using rxpatterns_type = std::vector<boost::wregex>;

private:
    class Iterator {

        struct Impl;

    public:
        using value_type = paths_type;

    private:
        boost::intrusive_ptr<Impl> pimpl_;
    };

public:
    using iterator = Iterator;
    using const_iterator = Iterator;

    struct InitParams {
        hash_algo algo;
        size_t block_size;
        size_t file_min_size;
        paths_type paths_scan;
        paths_type paths_exclude;
        rxpatterns_type rxpatterns;
    };

public:
    explicit SearchEngine(InitParams init_params);

    ~SearchEngine();

    void run(bool recursive);

private:
    boost::intrusive_ptr<Impl> pimpl_;
};

} // namespace griha