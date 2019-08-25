/// @file   search_engine.cpp
/// @brief  This file contains definition of SearchEngine class.
/// @author griha

#include "search_engine.h"

#include <iostream>
#include <stdexcept>
#include <cstdio>

#include <boost/bind.hpp>
#include <boost/container/map.hpp>
#include <boost/container/slist.hpp>
#include <boost/tuple/tuple.hpp>
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

bool is_excluded(const fs::path& path,
                 const fs::path& path_exclude_from,
                 const SearchEngine::paths_type& paths_exclude) {

    if (paths_exclude.empty())
        return false;

    const auto p = fs::relative(path, path_exclude_from);
    const auto it = rng::find_if(paths_exclude, [&lhs = p] (const fs::path& rhs) {
        return rng::search(lhs, rhs) != lhs.end();
    });
    return it != paths_exclude.end();
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
        if (!boost::regex_match(p.filename().string(), pattern))
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
        cont::slist<fs::path> files;
        nodes_type childs;
    };
    using roots_type = cont::map<uintmax_t, Node>;

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

    fs::path path_exclude_from;

    std::vector<char> buffer;
    roots_type roots;

    void clear();

    /// @brief Perfomrs hash function on block specified by @c level arguments
    /// @param ifs Input file stream
    /// @param level Value of level to describe a block to be hashed
    /// @return Digest value in base64 format
    /// @note Returns constant reference on @hash_sink member
    const std::string& hash_block(fs::ifstream& ifs, size_t level);

    void pre_process(const fs::path& file_path);
    Node& process(fs::ifstream& ifs, Node& n, size_t level);
    void process(const fs::path& file_path);
    void run(bool recursive);
};


struct SearchEngine::Iterator::Accessor::Impl {
    using node_type = SearchEngine::Impl::Node;
    const node_type* node;
};

struct SearchEngine::Iterator::Impl {
    using node_type = Accessor::Impl::node_type;
    using nodes_type = SearchEngine::Impl::nodes_type;
    using roots_type = SearchEngine::Impl::roots_type;
    
    const roots_type& roots;
    Accessor::Impl accessor;
    roots_type::const_iterator root_it;
    cont::slist<typename nodes_type::const_iterator> path;

    explicit Impl(const roots_type& r);
    Impl(const roots_type& r, const typename roots_type::const_iterator& it);

    void lookup_end_at_left();
    void next();
};

void SearchEngine::Impl::clear() {
    roots.clear();
}

const std::string& SearchEngine::Impl::hash_block(fs::ifstream& ifs, size_t level) {
    assert(ifs.is_open() && ifs.good());

    auto offset = level * block_size;
    if (ifs.tellg() != offset)
        ifs.seekg(offset, std::ios_base::beg);
    assert(ifs.good());

    ifs.read(buffer.data(), block_size);
    if (ifs.eof())
        rng::fill(buffer | boost::adaptors::sliced(ifs.gcount(), block_size), '\0');

    hash_sink.clear(); // actually this call never reduces the capacity of string
    hash_filter.PutMessageEnd(reinterpret_cast<uint8_t*>(buffer.data()), block_size);
    return hash_sink;
}

void SearchEngine::Impl::pre_process(const fs::path& file_path) {
    if (is_excluded(file_path, path_exclude_from, paths_exclude) ||
            !fs::is_regular_file(file_path))
        return;

    process(file_path);
}

SearchEngine::Impl::Node& SearchEngine::Impl::process(fs::ifstream& ifs, Node& n, size_t level) {
    assert(ifs.good() && n.files.empty() != n.childs.empty());

    if (n.childs.empty()) {
        fs::ifstream ifs_to_compare(n.files.front(), std::ios_base::binary|std::ios_base::in);
        ifs_to_compare.rdbuf()->pubsetbuf(buffer.data(), block_size);

        auto block_to_compare = hash_block(ifs_to_compare, level);
        auto& nn = n.childs[std::move(block_to_compare)];
        nn.files.swap(n.files);
    }

    auto block = hash_block(ifs, level);
    return n.childs[std::move(block)];
}


void SearchEngine::Impl::process(const fs::path& file_path) {
    if (!match_any(file_path, rxpatterns))
        return;
    
    auto file_size = fs::file_size(file_path);
    if (file_size < file_min_size)
        return;

    auto it = roots.find(file_size);
    if (it == roots.end()) {
        // no comparison required
        auto& n = roots[file_size];
        n.files.push_front(file_path);
        return;
    }

    fs::ifstream ifs(file_path, std::ios_base::binary|std::ios_base::in);
    ifs.rdbuf()->pubsetbuf(buffer.data(), block_size);

    size_t level = 0;
    for (auto n = &it->second;; 
         n = &process(ifs, *n, level), ++level) {
        if (ifs.eof() || (n->files.empty() && n->childs.empty())) {
            n->files.push_front(file_path);
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

        path_exclude_from = path;

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

SearchEngine::Iterator::Impl::Impl(const roots_type& r) 
    : roots(r)
    , accessor(Accessor::Impl { nullptr })
    , root_it(r.end()) {}

SearchEngine::Iterator::Impl::Impl(const roots_type& r, const typename roots_type::const_iterator& it) 
    : roots(r)
    , accessor(Accessor::Impl { &it->second })
    , root_it(it) {}

void SearchEngine::Iterator::Impl::lookup_end_at_left() {
    if (root_it == roots.end()) {
        accessor.node = nullptr;
        return;
    }

    if (!path.empty()) {
        auto& n = path.front()->second;
        accessor.node = &n;

        if (!n.files.empty())
            return;
        
        assert(!n.childs.empty());
        path.push_front(n.childs.begin());
    } else if (root_it->second.files.empty()) {
        path.push_front(root_it->second.childs.begin());
    } else {
        accessor.node = &root_it->second;
        return;
    }

    lookup_end_at_left();
}

void SearchEngine::Iterator::Impl::next() {
    if (root_it == roots.end())
        return; // stay on end iterator forever

    if (path.empty()) {
        if (root_it->second.childs.empty())
            ++root_it;
        else
            path.push_front(root_it->second.childs.begin());

        lookup_end_at_left();
        return;
    }

    auto it = path.front();

    // find next element fits for end lookup procedure
    if (it->second.childs.empty()) {
        // go right or go up and right
        path.pop_front();
        for (++it; 
             it != root_it->second.childs.end() &&
                !path.empty() &&
                it == path.front()->second.childs.end();
             ++it) {
            it = path.front();
            path.pop_front();
        }
        if (it == root_it->second.childs.end())
            ++root_it;
        else
            path.push_front(it);
    } else {
        // go down
        path.push_front(it->second.childs.begin());
    }
    // do lookup
    lookup_end_at_left();
} 

SearchEngine::Iterator::Accessor::~Accessor() = default;

SearchEngine::Iterator::Accessor::Accessor(Impl* impl)
    : pimpl_(impl) {}

SearchEngine::Iterator::Accessor::Accessor(Accessor&& src) {
    pimpl_.swap(src.pimpl_);
}

auto SearchEngine::Iterator::Accessor::operator= (Accessor&& rhs) -> Accessor& {
    pimpl_.swap(rhs.pimpl_);
    return *this;
}

SearchEngine::Iterator::Accessor::Accessor(const Accessor& src)
    : pimpl_(new Impl { *src.pimpl_ }) {}

auto SearchEngine::Iterator::Accessor::operator= (const Accessor& rhs) -> Accessor& {
    pimpl_.reset(new Impl { *rhs.pimpl_ } );
    return *this;
}

void SearchEngine::Iterator::Accessor::visit(const visitor_type& visitor) const {
    if (pimpl_->node == nullptr)
        throw std::logic_error("bad access");

    for (const auto& path : pimpl_->node->files)
        visitor(path);
}

SearchEngine::Iterator::~Iterator() = default;

SearchEngine::Iterator::Iterator(Impl* impl)
    : pimpl_(impl) {}

SearchEngine::Iterator::Iterator(Iterator&& src) {
    pimpl_.swap(src.pimpl_);
}

SearchEngine::Iterator::Iterator(const Iterator& src)
    : pimpl_(new Impl { *src.pimpl_ }) {}

auto SearchEngine::Iterator::operator= (Iterator&& rhs) -> Iterator& {
    pimpl_.swap(rhs.pimpl_);
    return *this;
}

auto SearchEngine::Iterator::operator= (const Iterator& rhs) -> Iterator& {
    pimpl_.reset(new Impl { *rhs.pimpl_ });
    return *this;
}

auto SearchEngine::Iterator::operator++() -> Iterator& {
    pimpl_->next();
    return *this;
}

auto SearchEngine::Iterator::operator*() -> value_type {
    return Accessor { new Accessor::Impl { pimpl_->accessor } };
}

bool operator== (const SearchEngine::Iterator& lhs, const SearchEngine::Iterator& rhs) {
    return lhs.pimpl_->root_it == rhs.pimpl_->root_it &&
            lhs.pimpl_->path == rhs.pimpl_->path;
}

SearchEngine::~SearchEngine() = default;

SearchEngine::SearchEngine(InitParams init_params)
    : pimpl_(new Impl { std::move(init_params) }) {}

auto SearchEngine::begin() const -> const_iterator {
    if (pimpl_->roots.empty())
        return Iterator { new Iterator::Impl { pimpl_->roots } };
    else
        return Iterator { new Iterator::Impl { pimpl_->roots, pimpl_->roots.begin() } };
}

auto SearchEngine::end() const -> const_iterator {
    return Iterator { new Iterator::Impl { pimpl_->roots, pimpl_->roots.end() } };
}

void SearchEngine::run(bool recursive) {
    pimpl_->run(recursive);
}

} // namespace griha