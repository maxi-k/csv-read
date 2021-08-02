#pragma once

#include <utest/utest.h>
#include "../util.hpp"

using namespace io;

UTEST(MMap, MapsFiles) {
    MMapping map("test/data/mmap-zerofile.bin");

    ASSERT_NE(-1, map.handle);
    ASSERT_NE(nullptr, map.data());
    ASSERT_EQ(512u, map.size);
    ASSERT_EQ(0, *map.data());

    map.~MMapping();

    ASSERT_EQ(nullptr, map.data());
    ASSERT_EQ(-1, map.handle);
}

UTEST(FixedSizeDataColumn, MapsFiles) {

}

UTEST(StringColumn, MapsFiles) {

}
