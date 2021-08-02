#pragma once

#include <x86intrin.h>

#include <string>
#include <vector>
#include <thread>

#include "util.hpp"

namespace io::csv {

    struct CharIter {
        const char* iter;
        const char* limit;

        template<typename T>
        static CharIter from_iterable(const T& iter) {
            return CharIter{iter.begin(), iter.end()};
        }
    };

    namespace {

        template<char delim>
        inline void find(CharIter& pos) {
            auto&& [iter, limit] = pos;
            const __m256i search_mask = _mm256_set1_epi8(delim);
            auto limit32 = limit - 32;
            while (iter < limit32) {
                auto block = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(iter));
                uint32_t matches = _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, search_mask));
                if (matches) {
                    iter += __builtin_ctz(matches);
                    return;
                } else {
                    iter += 32;
                }
            }
            while ((iter != limit) && ((*iter) != delim)) {
                ++iter;
            }
        }

        template<char delim>
        inline void find_nth(CharIter& pos, unsigned n) {
            auto&& [iter, limit] = pos;
            const __m256i search_mask = _mm256_set1_epi8(delim);
            auto limit32 = limit - 32;
            while (iter < limit32) {
                auto block = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(iter));
                uint32_t matches = _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, search_mask));
                if (matches) {
                    unsigned hits = _mm_popcnt_u32(matches);
                    if (hits < n) {
                        n -= hits;
                        iter += 32;
                    } else {
                        while (n > 1) {
                            matches &= matches - 1;
                            --n;
                        }
                        iter += __builtin_ctz(matches);
                        return;
                    }
                } else {
                    iter += 32;
                }
            }
            for (; iter != limit && n; ++iter) {
                n -= ((*iter) == delim);
            }
            iter -= iter != limit;
        }

        template<char delim>
        inline size_t parse_int(CharIter& pos) {
            auto&& [iter, limit] = pos;
            size_t v = *iter++ - '0';
            for (;iter != limit; ++iter) {
                char c = *iter;
                if (c == '|') break;
                v = 10 * v + c - '0';
            }
            return v;
        }

        template <typename Executor>
        inline void parallel_exec(const Executor &executor, unsigned thread_count = std::thread::hardware_concurrency() / 2) {
            std::vector<std::thread> threads;
            for (auto thread_id = 1u; thread_id != thread_count; ++thread_id) {
                threads.push_back(std::thread([thread_id, thread_count, &executor]() {
                    executor(thread_id, thread_count);
                }));
            }
            executor(0, thread_count);
            for (auto &thread : threads) {
                thread.join();
            }
        }

    } // namsepace

    template<char delim = ',', char eol = '\n', typename Consumer>
    inline bool read_line(CharIter& pos, std::initializer_list<unsigned> cols, const Consumer& consumer) {
        unsigned skipped = 0;
        for (auto col : cols) {
            auto n = col - skipped;
            switch(n) {
                case 0:
                    break;
                case 1:
                    find<delim>(pos);
                    break;
                default:
                    find_nth<delim>(pos, n);
            }
            if (pos.iter >= pos.limit) { return false; }
            pos.iter += *pos.iter == delim;
            consumer(col, pos);
            skipped = col + 1;
            ++pos.iter;
        }
        if (pos.iter < pos.limit && *pos.iter != eol) { find<eol>(pos); }
        pos.iter += pos.iter != pos.limit;
        return true;
    }

    template<char delim = ',', char eol = '\n', typename Consumer>
    inline std::size_t read_file(const char* filename, std::initializer_list<unsigned> cols, const Consumer& consumer) {
       MMapping<char> input(filename);
       auto pos = CharIter::from_iterable(input);
       std::size_t lines{0};
       while (read_line<delim, eol, Consumer>(pos, cols, consumer)) { ++lines; }
       return lines;
    }
}
