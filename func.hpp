#pragma once

#include "const.hpp"
#include <fstream>
#include <vector>

namespace RiscV {

struct DataToWrite {
    std::string filename = "";
    uint8_t* buff = nullptr;
    uint32_t len = 0;
    std::vector<uint32_t> regs;
    uint32_t addres = 0;
};

uint32_t GetTag(uint32_t addres) {
    return addres >> (CACHE_INDEX_LEN + CACHE_OFFSET_LEN);
}

uint32_t GetInd(uint32_t addres) {
    return (addres >> CACHE_OFFSET_LEN) & ((1UL << CACHE_INDEX_LEN) - 1UL);
}

uint32_t GetOffset(uint32_t addres) {
    return addres & ((1UL << CACHE_OFFSET_LEN) - 1UL);
}

uint32_t GetAddres(uint32_t tag, uint32_t ind) {
    return (tag << (CACHE_OFFSET_LEN + CACHE_INDEX_LEN)) | (ind << CACHE_OFFSET_LEN);
}

uint32_t GetOpcode(uint32_t instr) {
    return instr & ((1UL << 7UL) - 1UL);
}

uint32_t GetRd(uint32_t instr) {
    return (instr >> 7UL) & ((1UL << 5UL) - 1UL);
}

uint32_t GetFunct3(uint32_t instr) {
    return (instr >> 12UL) & ((1UL << 3UL) - 1UL);
}

uint32_t GetRs1(uint32_t instr) {
    return (instr >> 15UL) & ((1UL << 5UL) - 1UL);
}

uint32_t GetRs2(uint32_t instr) {
    return (instr >> 20UL) & ((1UL << 5UL) - 1UL);
}

uint32_t GetFunct7(uint32_t instr) {
    return (instr >> 25UL) & ((1UL << 7UL) - 1UL);
}

int32_t GetImmIType(uint32_t instr) {
    return static_cast<int32_t>(instr) >> 20UL;;
}

int32_t GetImmSType(uint32_t instr) {
    uint32_t raw = ((instr >> 25UL) << 5UL) | ((instr >> 7UL) & ((1UL << 5UL) - 1UL));
    return static_cast<int32_t>(raw << 20UL) >> 20UL;;
}

int32_t GetImmBType(uint32_t instr) {
    uint32_t raw = ((instr >> 31UL) << 12UL) | (((instr >> 7UL) & 1UL) << 11UL) | (((instr >> 25UL) & ((1UL << 6UL) - 1UL)) << 5UL) | (((instr >> 8UL) & ((1UL << 4UL) - 1UL)) << 1UL);

    return static_cast<int32_t>(raw << 19UL) >> 19UL;;
}

uint32_t GetImmUType(uint32_t instr) {
    return instr & 0xFFFFF000UL;
}

int32_t GetImmJType(uint32_t instr) {
    uint32_t raw = ((instr >> 31UL) << 20UL) | (((instr >> 12UL) & ((1UL << 8UL) - 1UL)) << 12UL) | (((instr >> 20UL) & 1UL) << 11UL) | (((instr >> 21UL) & ((1UL << 10UL) - 1UL)) << 1UL);
    return  static_cast<int32_t>(raw << 11UL) >> 11UL;
}

uint32_t GetShamt(uint32_t instr) {
    return (instr >> 20) & ((1UL << 5UL) - 1UL);
}

void FileWriter(const DataToWrite& data) {
    std::ofstream file(data.filename, std::ios::out | std::ios::binary | std::ios::trunc);
    for (size_t i = 0; i < 32; ++i) {
        file.write(reinterpret_cast<const char*>(&data.regs[i]), sizeof(uint32_t));
    }
    file.write(reinterpret_cast<const char*>(&data.addres), sizeof(data.addres));
    file.write(reinterpret_cast<const char*>(&data.len), sizeof(data.len));  
    file.write(reinterpret_cast<const char*>(data.buff + data.addres), data.len);
}
}
