#pragma once

#include <cassert>
#include <string>
#include <string_view>
#include <map>
#include <forward_list>
#include <algorithm>
#include <utility>
#include <optional>
#include <type_traits>
#include <iostream>

namespace griha {

namespace {

template <typename ToCharT, typename FromCharT>
std::basic_string<ToCharT> convert(std::basic_string_view<FromCharT> from);

template<>
std::wstring convert(std::string_view from) {
    std::mbstate_t state {};
    auto from_data = from.data();
    std::size_t l = std::mbsrtowcs(nullptr, &from_data, 0, &state);
    std::wstring ret(l, 0);
    std::mbsrtowcs(ret.data(), &from_data, ret.size(), &state);
    return ret;
}

} // unnamed namespace

template <typename CharT>
class radix_tree {

public:
    using string_type = std::basic_string<CharT>;
    using string_view_type = std::basic_string_view<CharT>;

private:
    struct node_type;
    using nodes_type = std::map<CharT, node_type>;
    struct node_type {
        string_type label;
        bool end_flag; // indicates end of word
        nodes_type childs;
    };

    struct iterator_impl {

        using value_type = std::pair<const string_type, const string_type>;

        const nodes_type& top_nodes;
        std::forward_list<typename nodes_type::const_iterator> path;
        mutable std::optional<value_type> value;

        iterator_impl(const nodes_type& tn, typename nodes_type::const_iterator it)
            : top_nodes(tn) {
            path.push_front(it);
        }

        void lookup_end_at_left() {
            assert(!path.empty());
            assert(path.front() != top_nodes.end());

            const auto& n = path.front()->second;
            if (n.end_flag) {
                return;
            }
            assert(!n.childs.empty());
            path.push_front(n.childs.begin());
            lookup_end_at_left();
        }

        void lookup_end_at_right() {
            assert(!path.empty());
            assert(path.front() != top_nodes.end());

            const auto& n = path.front()->second;
            if (n.end_flag) {
                return;
            }
            assert(!n.childs.empty());
            path.push_front(std::prev(n.childs.end()));
            lookup_end_at_right();
        }

        void next() {
            assert(!path.empty());

            auto it = path.front();
            if (it == top_nodes.end()) {
                return;
            }

            // find next element fits for end lookup procedure
            if (it->second.childs.empty()) {
                // go right or go up and right
                path.pop_front();
                for (++it; 
                     it != top_nodes.end() &&
                        !path.empty() &&
                        it == path.front()->second.childs.end();
                     ++it) {
                    it = path.front();
                    path.pop_front();
                }
                path.push_front(it);
                if (it == top_nodes.end()) {
                    return; // iterator has become end iterator
                }
            } else {
                // go down
                path.push_front(it->second.childs.begin());
            }
            // do lookup
            lookup_end_at_left();
        }

        void prev() {
            assert(!path.empty());
            
            if (top_nodes.empty()) {
                return;
            }

            // find next element fits for end lookup procedure
            auto it = path.front();
            if (it == top_nodes.end()) {
                --it;
            } else if (it->second.childs.empty()) {
                // go left or go up and left
                // if achieved begin of 
                path.pop_front();
                while (it != top_nodes.begin() &&
                        it == path.front()->second.childs.begin()) {
                    it = path.front();
                    path.pop_front();
                }
                if (it != top_nodes.begin()) {
                    --it;
                }
                path.push_front(it);
            } else {
                // go down
                path.push_front(std::prev(it->second.childs.end()));
            }
            // do lookup
            lookup_end_at_right();
        }

        void clear_value() {
            value.reset();
        }

        value_type& get_value() const {
            assert(!path.empty());

            if (path.front() == top_nodes.end()) {
                throw std::runtime_error("invalid iterator");
            }

            if (!value) {
                string_type suffix = path.front()->second.label, word;
                assert(!suffix.empty());
                for (auto it = std::next(path.begin());
                     it != path.end();
                     ++it) {
                    word = (*it)->second.label + word;
                }
                value.emplace(word + suffix, word + suffix[0]);
            }
            return value.value();
        }
    };

public:
    class iterator : iterator_impl {

        friend class radix_tree;

    public:
        using difference_type = ptrdiff_t;
        using typename iterator_impl::value_type;
        using reference = const value_type&;
        using pointer = const value_type*;
        using iterator_category = std::bidirectional_iterator_tag;

    public:
        iterator& operator++ () {
            iterator_impl::next();
            iterator_impl::clear_value();
            return *this;
        }

        iterator operator++ (int) {
            iterator ret(*this);
            ++(*this);
            return ret;
        }

        iterator& operator-- () {
            iterator_impl::prev();
            iterator_impl::clear_value();
            return *this;
        }

        iterator operator-- (int) {
            iterator ret(*this);
            --(*this);
            return ret;
        }

        reference operator* () const {
            return iterator_impl::get_value();
        }

        pointer operator-> () const {
            return &iterator_impl::get_value();
        }

        friend bool operator== (const iterator& lhs, const iterator& rhs) {
            return &lhs.top_nodes == &rhs.top_nodes && lhs.path == rhs.path;
        }

        friend bool operator!= (const iterator& lhs, const iterator& rhs) {
            return !(lhs == rhs);
        }

    private:
        iterator(const nodes_type& tn, typename nodes_type::const_iterator it)
            : iterator_impl{ tn, it } {}
    };
    using const_iterator = iterator;

public:
    void insert(string_view_type value) {
        if (!value.empty()) {
            insert(nodes[value[0]], value);
        }
    }

    template <typename CharU>
    void insert(std::basic_string_view<CharU> value) {
        const auto v = convert<CharT>(value);
        insert(v);
    }

    template <typename CharU, size_t N>
    void insert(CharU (&value)[N]) {
        insert(std::basic_string_view<std::decay_t<CharU>>(value, N - 1));
    }

    const_iterator begin() const { 
        const_iterator ret{ nodes, nodes.begin() };
        if (!nodes.empty()) {
            ret.lookup_end_at_left();
        }
        return ret;
    }
    const_iterator cbegin() const { return begin(); }

    const_iterator end() const { return const_iterator{ nodes, nodes.end() }; }
    const_iterator cend() const { return end(); }

private:
    void insert(node_type& n, string_view_type value) {
        if (n.label.empty()) {
            // node 'n' is a place for store 'value' - empty leaf
            n.label = value;
            n.end_flag = true;
            return;
        }

        // find mismatch of two strings
        auto [it_n, it_v] = std::mismatch(
            n.label.begin(), n.label.end(),
            value.begin(), value.end());

        if (it_n != n.label.end() ) {
            // get match prefix and create new node for replacing with node 'n'
            node_type new_n = { string_type{ n.label.begin(), it_n }, false, {} };
            n.label = string_type{ it_n, n.label.end() };
            new_n.childs.emplace(n.label[0], std::move(n));
            n = std::move(new_n);
        }

        if (it_v != value.end()) {
            // insert right part of new value to childs
            insert(n.childs[*it_v], value.substr(it_v - value.begin()));
        } else {
            // new value is prefix of exist word
            // mark it as end of word
            n.end_flag = true;
        }
    }

    static
    std::basic_ostream<CharT>&
    print(std::basic_ostream<CharT>& os,
          const node_type& node, bool last_node,
          string_view_type prefix) {

        os << prefix
           << os.widen('+') << os.widen(' ') 
           << node.label;
        if (node.end_flag) {
            os << os.widen('$');
        }
        os << os.widen('\n');

        string_type prefix_child { prefix.data(), prefix.size() };
        prefix_child += os.widen(last_node ? ' ' : '|');
        prefix_child += os.widen(' ');
        return radix_tree::print(os, node.childs, std::move(prefix_child));
    }
    
    static
    std::basic_ostream<CharT>&
    print(std::basic_ostream<CharT>& os, const nodes_type& nodes, string_type prefix = string_type{}) {
        if (nodes.empty()) {
            return os;
        }

        const auto it_pre_end = std::prev(nodes.end());
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            radix_tree::print(os, it->second, it == it_pre_end, prefix);
        }
        return os;
    }

    friend
    std::basic_ostream<CharT>& 
    operator<< (std::basic_ostream<CharT>& os, const radix_tree& rtree) {
        return radix_tree::print(os, rtree.nodes);
    }

private:
    nodes_type nodes;
};

} // namespace griha