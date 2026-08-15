// MIT License
// Copyright (c) 2021-2023  @guritchi
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#ifndef VKCPP_GEN_COLLECTION_HPP
#define VKCPP_GEN_COLLECTION_HPP

#ifndef USE_PCH
#include <vector>
#include <map>
#include <span>
#endif

#include "Utils.hpp"
#include "Types.hpp"

namespace vkgen
{
    struct Enum;

    template <typename T>
    class MutableCollection;

    // TODO rename Collection
    template <typename T>
    class Container
    {
        static_assert(std::is_base_of<GenericType, T>::value, "T must derive from BaseType");
    public:
        std::map<std::string, size_t, std::less<>> map;

#ifndef NDEBUG
        bool mapBuilt = false;
#endif
        void assertMap() const {
#ifndef NDEBUG
            if (!mapBuilt && !items.empty()) {
                throw std::runtime_error("Lookup called on Container<" + std::string{ typeid(T).name() } + "> that is not prepared");
            }
#endif
        }

        std::vector<T>                         items;

        friend MutableCollection<T>;

    public:
        using iterator       = std::vector<T>::iterator;
        using const_iterator = std::vector<T>::const_iterator;
        using value_type = T;

        std::vector<std::reference_wrapper<T>> ordered;
        std::vector<std::reference_wrapper<T>> vulkan; // TODO simplify

        bool contains(const std::string_view name) const {
            return find(name) != nullptr;
        }

        void addMapping(const String &key, size_t itemIndex) {
            map.emplace(key.original, itemIndex);
            map.emplace(key, itemIndex);
        }

        void buildOrder() {
            ordered.clear();
            ordered.reserve(items.size());
            vulkan.clear();
            vulkan.reserve(items.size());
            for (auto &item : items) {
                // if (item.isSupported()) {
                    ordered.emplace_back(std::ref(item));
                // }
                vulkan.emplace_back(std::ref(item));
            }
            std::sort(ordered.begin(), ordered.end(),
            [](const T &a, const T& b) -> bool {
                    return a < b;
                }
            );
        }

        void addTypes(std::unordered_map<std::string, GenericType *> &types) {
            for (const auto &k : map) {
                types.emplace(k.first, &items[k.second]);
            }
        }

        const T* find(const std::string_view name, bool dbg = false) const {
            assertMap();
            auto it = map.find(std::string{ name });
            if (it == map.end()) {
                if (dbg)
                    std::cerr << ". " << std::string{ name } << " not found in Container<" << std::string{ typeid(T).name() } << ">\n";
                return nullptr;
            }
            return &items[it->second];
        }

        T* find(const std::string_view name, bool dbg = false) {
            assertMap();
            auto it = map.find(std::string{ name });
            if (it == map.end()) {
                if (dbg)
                    std::cerr << ". " << std::string{ name } << " not found in Container<" << std::string{ typeid(T).name() } << ">\n";
                return nullptr;
            }
            return &items[it->second];
        }

        T &operator[](const std::string_view name) {
            assertMap();
            auto it = map.find(name);
            if (it == map.end()) {
                // for (const auto &m : map) {
                //     std::cout << m.first << "\n";
                // }
                throw std::runtime_error(std::string{ name } + " not found in Container<" + std::string{ typeid(T).name() } + ">");
            }
            return items[it->second];
        }

        const T &operator[](const std::string_view name) const {
            assertMap();
            auto it = map.find(name);
            if (it == map.end()) {
                // for (const auto &m : map) {
                //     std::cout << m.first << "\n";
                // }
                throw std::runtime_error(std::string{ name } + " not found in Container<" + std::string{ typeid(T).name() } + ">");
            }
            return items[it->second];
        }

        iterator end() {
            return items.end();
        }

        iterator begin() {
            return items.begin();
        }

        const_iterator end() const {
            return items.cend();
        }

        const_iterator begin() const {
            return items.cbegin();
        }

        void clear() {
            items.clear();
            ordered.clear();
            map.clear();
        }

        size_t size() const {
            return items.size();
        }

        void foreach(std::function<void(const String &, T &)> func) const {
            for (auto &item : items) {
                func(item.name, item);
                for (const auto &alias : item.aliased) {
                    func(alias, item);
                }
            }
        }

#ifndef NDEBUG
        void debugPrintMap() const {
            std::cout << "Collection<" << std::string{ typeid(T).name() } << ">: " << items.size() << ", map: " << map.size() << "\n";
            for (const auto &v : map) {
                std::cout << "Mapped enum: " << v.first << "\n";
            }
            // for (const auto &i : items) {
            //     if (!map.count(i.first)) {}
            // }
        }
#endif
    };

}

#endif  // VKCPP_GEN_COLLECTION_HPP
