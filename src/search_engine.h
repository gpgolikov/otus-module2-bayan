/// @file   search_engine.h
/// @brief  This file contains declaration of SearchEngine class provides base search
///         driver that searches for duplicates of files in specified locations.
/// @author griha

#pragma once

#include <vector>
#include <iterator>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>

namespace griha {

enum class hash_algo {
    md5,
    sha256
};

class SearchEngine {

    struct Impl;

public:

    class Iterator {

        friend class SearchEngine;

        struct Impl;

    public:
        class Accessor {
            
            friend class Iterator;

            struct Impl;

        public:
            using visitor_type = boost::function<void (const boost::filesystem::path&)>;

        public:
            ~Accessor();

            Accessor(const Accessor& src);
            Accessor(Accessor&& src);

            Accessor& operator= (const Accessor& rhs);
            Accessor& operator= (Accessor&& rhs);

            void visit(const visitor_type& visitor) const;

        private:
            explicit Accessor(Impl* impl);

        private:
            boost::scoped_ptr<Impl> pimpl_;
        };

        using difference_type = void;
        using value_type = Accessor;
        using iterator_category = std::forward_iterator_tag;

    public:
        ~Iterator();

        Iterator(const Iterator& src);
        Iterator(Iterator&& src);

        Iterator& operator= (const Iterator& rhs);
        Iterator& operator= (Iterator&& rhs);

        Iterator& operator++();
        value_type operator*();

        friend bool operator== (const Iterator& lhs, const Iterator& rhs);
        friend bool operator!= (const Iterator& lhs, const Iterator& rhs) {
            return !(lhs == rhs);
        }

    private:
        explicit Iterator(Impl* impl);
    private:
        boost::scoped_ptr<Impl> pimpl_;
    };

    using paths_type = std::vector<boost::filesystem::path>;
    using rxpatterns_type = std::vector<boost::regex>;

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

    const_iterator begin() const;
    const_iterator end() const;

    void run(bool recursive);

private:
    boost::intrusive_ptr<Impl> pimpl_;
};

} // namespace griha