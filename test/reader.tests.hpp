#pragma once

#include <utest/utest.h>
#include "../csv.hpp"

using namespace io::csv;

UTEST(Find, FindsDelimiters) {
    char str[] = "hello|world|";
    CharIter pos{str, str + sizeof(str)};
    find<'|'>(pos);
    ASSERT_EQ(str + 5, pos.iter);
    ++pos.iter;
    find<'|'>(pos);
    ASSERT_EQ(str + 11, pos.iter);
}

UTEST(Find, StopsAtEnd) {
    char str[] = "hello|world|";
    CharIter pos{str, str + sizeof(str)};
    find<','>(pos);
    ASSERT_EQ(str + sizeof(str), pos.iter);
    ASSERT_EQ(pos.limit, pos.iter);
}

UTEST(FindNth, FindsDelimiters) {
    char str[] = "hello|world|this|is|a|csv|library";
    CharIter pos{str, str + sizeof(str)};

    find_nth<'|'>(pos, 2);
    ASSERT_EQ(11, pos.iter - str);

    auto prev = pos.iter;
    find_nth<'|'>(pos, 1);
    ASSERT_EQ(11, pos.iter - str);
    ASSERT_EQ(prev, pos.iter);

    find_nth<'|'>(pos, 5);
    ASSERT_EQ(25, pos.iter - str);
}

UTEST(FindNth, StopsAtEnd) {
    char str[] = "hello|world|this|is|a|csv|library";
    CharIter pos{str, str + sizeof(str)};

    find_nth<'|'>(pos, 7);
    ASSERT_EQ(pos.iter - str, (int) sizeof(str));
    ASSERT_EQ(pos.limit, pos.iter);
}

UTEST(ReadLine, ProducesColumns) {
    char str[] = "hello|world|this|is|a|csv|library";
    CharIter pos{str, str + sizeof(str)};

    auto called = 0;
    read_line<'|'>(pos, {0, 1, 3, 6}, [&](unsigned column, CharIter& at) {
        ++called;
        switch(column) {
            case 0:
                ASSERT_EQ(0, at.iter - str);
                break;
            case 1:
                ASSERT_EQ(6, at.iter - str);
                break;
            case 3:
                ASSERT_EQ(17, at.iter - str);
                break;
            case 6:
                ASSERT_EQ(26, at.iter - str);
                break;
            default:
                ASSERT_TRUE(false);
                break;
        }
        find<'|'>(at);
    });

    ASSERT_EQ(4, called);
}

UTEST(ReadFile, ReadsInputFile) {
    char filename[] = "test/data/csv-testfile.csv";
    auto called = 0u;
    auto lines = read_file<','>(filename, {0, 1, 5}, [&](unsigned column, CharIter& at) {
      ++called;
      find<','>(at);
    });

    ASSERT_EQ(43u, lines);
    ASSERT_EQ(3 * 43u, called);
}


UTEST(ReadFile, ReadsInputFile2) {
    char filename[] = "test/data/csv-testfile2.csv";
    auto called = 0u, endcols = 0u;
    auto lines = read_file<','>(filename, {0, 5, 13}, [&](unsigned column, CharIter& at) {
      if (column == 0 && called != 0) {
          Parser<unsigned> p;
          unsigned v = p.parse_value<',', '\n'>(at);
          ASSERT_EQ(1u, v);
      }
      else if (column == 13) {
          ++endcols;
         Parser<std::string_view> p;
         std::string_view v = p.parse_value<',', '\n'>(at);
         std::array<char, 20> dest; dest.fill('\0');
         v.copy(dest.data(), v.size());
         ASSERT_STREQ("somestrln", dest.data());
      } else {
          find_either<',', '\n'>(at);
      }
      ++called;
    });

    ASSERT_EQ(43u, endcols);
    ASSERT_EQ(43u, lines);
    ASSERT_EQ(3 * 43u, called);
}
