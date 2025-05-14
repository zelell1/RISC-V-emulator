#pragma once

#include <iostream>

inline static constexpr size_t ADDRESS_LEN = 17; // – длина адреса (в битах)
inline static constexpr size_t CACHE_INDEX_LEN = 4; //– длина индекса блока кэш-линий  (в битах)
inline static constexpr size_t CACHE_LINE_SIZE = 64; // размер кэш-линии (в байтах)
inline static constexpr size_t CACHE_LINE_COUNT = 64; // кол-во кэш-линий
inline static constexpr size_t MEMORY_SIZE = 1 << ADDRESS_LEN; // 2^(ADDRESS LEN)
inline static constexpr size_t CACHE_SET_COUNT = 1 << CACHE_INDEX_LEN; // 2 ^ CACHE_INDEX_LEN
inline static constexpr size_t CACHE_OFFSET_LEN = 6; // log2(CACHE_LINE_SIZE)
inline static constexpr size_t CACHE_SIZE = CACHE_LINE_COUNT * CACHE_LINE_SIZE; // CACHE_LINE_COUNT * CACHE_LINE_SIZE  
inline static constexpr size_t CACHE_WAY = CACHE_LINE_COUNT / CACHE_SET_COUNT; // CACHE_LINE_COUNT / CACHE_SET_COUNT
inline static constexpr size_t CACHE_TAG_LEN = ADDRESS_LEN - CACHE_INDEX_LEN - CACHE_OFFSET_LEN; // ADDRESS_LEN - CACHE_INDEX_LEN - CACHE_OFFSET_LEN

inline static constexpr size_t OpcodeLen = 7;



enum class CRP {
    LRU, pLRU
};