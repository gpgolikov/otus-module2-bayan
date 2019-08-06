/// @file   search_engine.h
/// @brief  This file contains declaration of SearchEngine class provides base search
///         driver that is searching for duplicates of files in specified locations.
/// @author griha

#pragma once

#include <vector>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

namespace fs = boost::filesystem;

namespace griha {
        
class SearchEngine {

public:
    using paths_type = std::vector<fs::path>;
    using rxpatterns_type = std::vector<boost::wregex>;

    struct InitParams {
        paths_type paths_scan;
        paths_type paths_exclude;
        rxpatterns_type rxpatterns;
    };

public:
    explicit SearchEngine(InitParams init_params);

    void run();

private:
    paths_type paths_scan_;
    paths_type paths_exclude_;
    rxpatterns_type rxpatterns_;
};

} // namespace griha