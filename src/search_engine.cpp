/// @file   search_engine.cpp
/// @brief  This file contains definition of SearchEngine class.
/// @author griha

#include "search_engine.h"

#include <iostream>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/container/map.hpp>
#include <boost/container/slist.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/optional.hpp>

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md5.h>
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/base64.h>

namespace fs = boost::filesystem;
namespace cont = boost::container;
namespace rng = boost::range;

namespace griha {

namespace {

bool is_excluded(const fs::path& path, const SearchEngine::paths_type& excl_paths) {
    if (excl_paths.empty())
        return false;

    const auto p = fs::system_complete(path);
    const auto it = rng::find_if(excl_paths, [&lhs = path] (const fs::path& rhs) {
        return rng::mismatch(lhs, rhs).first == lhs.end();
    });
    return it != excl_paths.end();
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

CryptoPP::HashTransformation* make_hash(hash_algo algo) {
    switch (algo) {
    case hash_algo::md5:
        // return boost::scoped_ptr<CryptoPP::HashTransformation> { new CryptoPP::Weak::MD5 {} };
        return new CryptoPP::Weak::MD5 {};
    case hash_algo::sha256:
        return new CryptoPP::SHA256 {};
    }
    throw std::invalid_argument { "unknown hash agorithm" };
}

} // unnamed namespace

struct SearchEngine::Impl : boost::intrusive_ref_counter<SearchEngine::Impl, boost::thread_unsafe_counter> {

    struct Node;
    using nodes_type = cont::map<std::string, Node>;
    struct Node {
        paths_type duplicates;
        nodes_type childs;
        fs::path file_to_compare;
    };

    explicit Impl(SearchEngine::InitParams init_params)
        : block_size(init_params.block_size)
        , file_min_size(init_params.file_min_size)
        , paths_scan(std::move(init_params.paths_scan))
        , paths_exclude(std::move(init_params.paths_exclude))
        , rxpatterns(std::move(init_params.rxpatterns))
        , hash(make_hash(init_params.algo))
        , hash_filter(*hash, new CryptoPP::Base64Encoder(new CryptoPP::StringSink(hash_sink), false))
        , buffer(init_params.block_size) {}

    const size_t block_size;
    const size_t file_min_size;
    const SearchEngine::paths_type paths_scan;
    const SearchEngine::paths_type paths_exclude;
    const SearchEngine::rxpatterns_type rxpatterns;

    /// @name hashing support fields
    /// @note order of these fields initialization is important
    /// @{
    boost::scoped_ptr<CryptoPP::HashTransformation> hash;
    std::string hash_sink;
    CryptoPP::HashFilter hash_filter;
    /// @}

    std::vector<char> buffer;
    Node root;

    void clear();

    /// @brief Perfomrs hash function on block specified by @c level arguments
    /// @param is Input filestream
    /// @param level Value of level to describe a block to be hashed
    /// @return Digest value in base64 format
    /// @note Returns constant reference on @hash_sink member
    const std::string& hash_block(std::ifstream& is, size_t level);

    void pre_process(const fs::path& file_path);
    void process(const fs::path& file_path);
    void run(bool recursive);
};

struct SearchEngine::Iterator::Impl {
    using node_type = SearchEngine::Impl::Node;
    using nodes_type = SearchEngine::Impl::nodes_type;

    const node_type& root;
    cont::slist<typename nodes_type::const_iterator> path;
    bool on_file_to_compare { false };

    explicit Impl(const node_type& r) 
        : root(r) {}

    Impl(const node_type& r, typename nodes_type::const_iterator& it) 
        : root(r) {
        path.push_front(it);
    }

    void lookup_end_at_left();
    void next();
};

struct SearchEngine::Iterator::Accessor::Impl {
    const Iterator::Impl::node_type& root;
    const boost::optional<Iterator::Impl::nodes_type::const_iterator> iterator;
    const bool on_file_to_compare;
};

void SearchEngine::Impl::clear() {
    root.duplicates.clear();
    root.childs.clear();
    root.file_to_compare.clear();
}

const std::string& SearchEngine::Impl::hash_block(std::ifstream& is, size_t level) {
    assert(is.is_open() && is.good());

    is.seekg(level * block_size, std::ios_base::beg);
    assert(is.good());

    is.read(buffer.data(), block_size);
    if (is.eof())
        rng::fill(buffer | boost::adaptors::sliced(is.gcount(), block_size), '\0');

    hash_sink.clear(); // actually this call never reduces the capacity of string
    hash_filter.PutMessageEnd(reinterpret_cast<CryptoPP::byte*>(buffer.data()), block_size);
    return hash_sink;
}

void SearchEngine::Impl::pre_process(const fs::path& file_path) {
    if (is_excluded(file_path, paths_exclude) ||
            !fs::is_regular_file(file_path))
        return;

    process(file_path);
}

void SearchEngine::Impl::process(const fs::path& file_path) {
    if (!match_any(file_path, rxpatterns) ||
            fs::file_size(file_path) < file_min_size)
        return;

    fs::ifstream ifs { file_path };

    size_t level = 0;
    for (auto n = &root;;++level) {
        if (!n->file_to_compare.empty()) {
            fs::ifstream ifs_to_compare { n->file_to_compare };
            auto block_to_compare = hash_block(ifs_to_compare, level);
            if (ifs_to_compare.eof()) {
                n->childs[std::move(block_to_compare)].duplicates.push_back(n->file_to_compare);
                n->file_to_compare.clear();
            } else
                n->childs[std::move(block_to_compare)].file_to_compare.swap(n->file_to_compare);
        } else if (n->childs.empty()) {
            n->file_to_compare = file_path;
            break;
        }

        auto block = hash_block(ifs, level);
        n = &n->childs[std::move(block)];

        if (ifs.eof()) {
            n->duplicates.push_back(file_path);
            break;
        }
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
                fs::recursive_directory_iterator{path}, fs::recursive_directory_iterator{},
                boost::bind(&Impl::pre_process, this, boost::placeholders::_1));
        else
            std::for_each(
                fs::directory_iterator{path}, fs::directory_iterator{},
                boost::bind(&Impl::pre_process, this, boost::placeholders::_1));
    }
}

void SearchEngine::Iterator::Impl::lookup_end_at_left() {
    assert(!path.empty());
    assert(path.front() != root.childs.end());

    const auto& n = path.front()->second;
    if (!n.duplicates.empty()) {
        on_file_to_compare = false;
        return;
    }
    
    if (!n.file_to_compare.empty()) {
        on_file_to_compare = true;
        return;
    }
    
    assert(!n.childs.empty());
    path.push_front(n.childs.begin());
    lookup_end_at_left();
}

void SearchEngine::Iterator::Impl::next() {
    if (path.empty()) { // achieved end iterator in empty collection
        path.push_front(root.childs.end());
        on_file_to_compare = false;
        return;
    }

    auto it = path.front();
    if (it == root.childs.end())
        return; // stay on end iterator forever

    if (!on_file_to_compare && !it->second.file_to_compare.empty()) {
        on_file_to_compare = true;
        return; // just move to file_to_compare
    }

    // find next element fits for end lookup procedure
    if (it->second.childs.empty()) {
        // go right or go up and right
        path.pop_front();
        for (++it; 
                it != root.childs.end() &&
                !path.empty() &&
                it == path.front()->second.childs.end();
                ++it) {
            it = path.front();
            path.pop_front();
        }
        path.push_front(it);
        if (it == root.childs.end()) {
            return; // iterator has become end iterator
        }
    } else {
        // go down
        path.push_front(it->second.childs.begin());
    }
    // do lookup
    lookup_end_at_left();
} 

SearchEngine::~SearchEngine() = default;

SearchEngine::SearchEngine(InitParams init_params)
    : pimpl_(new Impl { std::move(init_params) }) {}

void SearchEngine::run(bool recursive) {
    pimpl_->run(recursive);
}

SearchEngine::Iterator::~Iterator() = default;

SearchEngine::Iterator::Iterator(Impl* impl)
    : pimpl_(impl) {}

auto SearchEngine::Iterator::operator++() -> Iterator& {
    pimpl_->next();
    return *this;
}

auto SearchEngine::Iterator::operator*() -> reference {
    if (pimpl_->path.empty())
}

pointer operator->();

friend bool operator== (const Iterator& lhs, const Iterator& rhs);

} // namespace griha