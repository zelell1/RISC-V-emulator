#pragma once

#include <fstream>
#include <vector>
#include <string>
#include <iostream>


struct fragment {
    uint32_t addres;
    std::vector<uint8_t> data;
};

class BinParser {
public:
    BinParser(const std::string& filename) : regs_(32) {
        bin_parse(filename);
    }

    std::vector<uint32_t> regs_;
    std::vector<fragment> frag_ram_;

private:
    void bin_parse(const std::string& filename) {
    std::ifstream bin(filename, std::ios::binary);
    for (int i = 0; i < 32; ++i) {
        uint32_t value;
        bin.read(reinterpret_cast<char*>(&value), sizeof(value));
        regs_[i] = value;
    }
    while (true) {
        fragment frag;
        uint32_t n;
        bin.read(reinterpret_cast<char*>(&frag.addres), sizeof(frag.addres));
        if (!bin) {
             break;
        }
        bin.read(reinterpret_cast<char*>(&n), sizeof(n));
        if (!bin) {
            break;
        }
        frag.data.resize(n);    
        bin.read(reinterpret_cast<char*>(frag.data.data()), n);
        if (!bin) { 
            break;
        }
        frag_ram_.push_back(std::move(frag));
    }
}
};

