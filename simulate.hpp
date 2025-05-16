#pragma once

#include "const.hpp"
#include "func.hpp"
#include "bin_parser.hpp"
#include "parser.hpp"
#include <vector>
#include <array>
#include <list>
#include <algorithm>
#include <cstdio>
#include <string>

namespace RiscV {

class RAM {
private:
    std::vector<uint8_t> ram_;

public:
    RAM(const std::vector<fragment>& frag) : ram_(MEMORY_SIZE) {
        for (const auto& el : frag) {
            for (int i = 0; i < el.data.size(); ++i) {
                ram_[el.addres + i] = el.data[i];
            }
        }
    };

    std::array<uint8_t, CACHE_LINE_SIZE> ReadRAM(uint32_t address) {
        std::array<uint8_t, CACHE_LINE_SIZE> result;
        std::copy(ram_.begin() + address, ram_.begin() + address + CACHE_LINE_SIZE, result.begin());
        return result;
    }

    void WriteRAM(uint32_t addres, std::array<uint8_t, CACHE_LINE_SIZE> data) {
        std::copy(data.begin(), data.begin() + CACHE_LINE_SIZE, ram_.begin() + addres);
    }

    uint8_t* GetData() {
        return ram_.data();
    }
};

class CacheLine {
public:
    std::array<uint8_t, CACHE_LINE_SIZE> data_;
    bool is_dirty;
    bool is_valid;
    bool plru;
    uint32_t tag;

    CacheLine() : is_dirty(false), is_valid(false), plru(false), tag(UINT32_MAX) {};

    template <typename T>
    T ReadCacheLine(uint32_t offset) {
        T result;
        std::memcpy(&result, &data_[offset], sizeof(T));
        return result;
    }

    template <typename T>
    void WriteCacheLine(uint32_t offset, T value) {
        is_valid = true;
        is_dirty = true;
        std::memcpy(&data_[offset], &value, sizeof(T));
    }

    void UpdateCacheLine(const std::array<uint8_t, CACHE_LINE_SIZE>& data, uint32_t new_tag){
        data_ = data;
        is_dirty = false;
        tag = new_tag;
        is_valid = true;
    }
};

class LruPolicy {
public:
    static uint32_t GetNextLine(std::array<CacheLine, CACHE_WAY>& lines, std::list<uint32_t>& lru_list) {
        if (lru_list.size() < CACHE_WAY) {
            return lru_list.size();
        }
        return lru_list.front();
    }

    static void UpdateLines(uint32_t line, std::list<uint32_t>& lru_list) {
        lru_list.remove(line);
        lru_list.push_back(line);
    }
};

class bpLruPolicy {
public:
    static uint32_t GetNextLine(std::array<CacheLine, CACHE_WAY>& lines) {
        for (size_t i = 0; i < CACHE_WAY; ++i) {
            if (!lines[i].plru) {
                return i;                                                                   
            }
        }
        return 0;
    }

    static void UpdateLines(uint32_t line, std::array<CacheLine, CACHE_WAY>& lines) {
        lines[line].plru = true;
        bool all_busy = true;
        for (uint32_t i = 0; i < CACHE_WAY; ++i) {
            if (!lines[i].plru) {
                all_busy = false;
                
            }
        }
        if (all_busy) {
            for (uint32_t i = 0; i < CACHE_WAY; ++i) {
                lines[i].plru = false;
            }
        }   
        lines[line].plru = true; 
    }
};

template<CRP T> 
class CacheSet;

template<> 
class CacheSet<CRP::LRU> {
public:
    std::array<CacheLine, CACHE_WAY> lines;
    std::list<uint32_t> list_idx;

    bool IsHit(uint32_t tag, uint32_t& ind) {
        for (size_t i = 0; i < CACHE_WAY; ++i) {
            if (lines[i].is_valid && lines[i].tag == tag) {
                ind = i;
                return true;
            }
        }
        return false;
    }

    bool IsValid(uint32_t ind) {
        return lines[ind].is_valid;
    }

    bool IsDirty(uint32_t ind) {
        return lines[ind].is_dirty;
    }

    uint32_t GetNextLine() {
        return LruPolicy::GetNextLine(lines, list_idx);
    }

    template <typename U>
    U Read(uint32_t line, uint32_t tag, uint32_t offset) {
        LruPolicy::UpdateLines(line, list_idx);
        return lines[line].template ReadCacheLine<U>(offset);
    }

    template <typename U>
    void Write(uint32_t line, uint32_t tag, uint32_t offset, U value) {
        LruPolicy::UpdateLines(line, list_idx);
        return lines[line].template WriteCacheLine<U>(offset, value);
    }
};

template<> 
class CacheSet<CRP::pLRU> {
public:
    std::array<CacheLine, CACHE_WAY> lines;

    bool IsHit(uint32_t tag, uint32_t& ind) {
        for (size_t i = 0; i < CACHE_WAY; ++i) {
            if (lines[i].is_valid && lines[i].tag == tag) {
                ind = i;
                return true;
            }
        }
        return false;
    }

    bool IsValid(uint32_t ind) {
        return lines[ind].is_valid;
    }

    bool IsDirty(uint32_t ind) {
        return lines[ind].is_dirty;
    }

    uint32_t GetNextLine() {
        return bpLruPolicy::GetNextLine(lines);
    }

    template <typename U>
    U Read(uint32_t line, uint32_t tag, uint32_t offset) {
        bpLruPolicy::UpdateLines(line, lines);
        return lines[line].template ReadCacheLine<U>(offset);
    }

    template <typename U>
    void Write(uint32_t line, uint32_t tag, uint32_t offset, U value) {
        bpLruPolicy::UpdateLines(line, lines);
        return lines[line].template WriteCacheLine<U>(offset, value);
    }
};

template<CRP T>
class CacheController {
private:
    std::array<CacheSet<T>, CACHE_SET_COUNT> data;
    size_t hits_inst_, hits_data_, inst_cnt_, data_cnt_;

    void UpdateСnt(bool is_data) {
        if (is_data) {
            ++data_cnt_;
        } else {
            ++inst_cnt_;
        }
    }

    void UpdateHits(bool is_data) {
        if (is_data) {
            ++hits_data_;
        } else {
            ++hits_inst_;
        }
    }

    void WriteBackLine(auto& set, uint32_t line, uint32_t index, RAM& ram) {
        if (set.IsDirty(line) && set.IsValid(line)) {
            uint32_t old_addr = (set.lines[line].tag << (CACHE_INDEX_LEN + CACHE_OFFSET_LEN)) | (index << CACHE_OFFSET_LEN);
            ram.WriteRAM(old_addr, set.lines[line].data_);
        }
    }

    uint32_t UpdateLine(auto& set, uint32_t tag, uint32_t index, RAM& ram) {
        uint32_t new_line = set.GetNextLine();
        WriteBackLine(set, new_line, index, ram);
        auto block = ram.ReadRAM(GetAddres(tag, index));
        set.lines[new_line].UpdateCacheLine(block, tag);
        return new_line;
    }

public:
    CacheController() : hits_inst_(0), hits_data_(0), inst_cnt_(0), data_cnt_(0) {};

    template<typename U>
    U ReadFromCache(uint32_t addres, bool is_data, RAM& ram) {
        uint32_t tag = GetTag(addres);
        uint32_t index = GetInd(addres);
        uint32_t offset = GetOffset(addres);
        auto& curr_set = data[index];
        uint32_t ind;
        UpdateСnt(is_data);
        if (curr_set.IsHit(tag, ind)) {
            UpdateHits(is_data);
        } else {
            ind = UpdateLine(curr_set, tag, index, ram);
        }
        return curr_set.template Read<U>(ind, tag, offset);
    }    

    template <typename U>
    void WriteInCache(uint32_t addres, bool is_data, U value, RAM& ram) {
        uint32_t tag = GetTag(addres);
        uint32_t index = GetInd(addres);
        uint32_t offset = GetOffset(addres);
        auto& curr_set = data[index];
        uint32_t ind;
        UpdateСnt(is_data);
        if (curr_set.IsHit(tag, ind)) {
            UpdateHits(is_data);
        } else {
            ind = UpdateLine(curr_set, tag, index, ram);
        }
        curr_set.template Write<U>(ind, tag, offset, value);
    }

    void ClearCache(RAM& ram) {
        for (uint32_t i = 0; i < CACHE_SET_COUNT; ++i) {
            auto& curr_set = data[i];
            for (uint32_t j = 0; j < CACHE_WAY; ++j) {
                WriteBackLine(curr_set, j, i, ram);
                curr_set.lines[j].is_dirty = false;
                curr_set.lines[j].is_valid = false;
                curr_set.lines[j].plru = false;
                curr_set.lines[j].tag = UINT32_MAX;
            } 
        }
    }

    void PrintRate();
};

template<>
void CacheController<CRP::LRU>::PrintRate()  {
    printf("        LRU\t%3.5f%%\t%3.5f%%\t%3.5f%%\n", std::abs((100.0 * (hits_data_ + hits_inst_)) / (inst_cnt_ + data_cnt_)), std::abs(100.0 * hits_inst_ / inst_cnt_), std::abs(100.0 * hits_data_ / data_cnt_));
}

template<>
void CacheController<CRP::pLRU>::PrintRate() {
    printf("      bpLRU\t%3.5f%%\t%3.5f%%\t%3.5f%%\n", std::abs((100.0 * (hits_data_ + hits_inst_)) / (inst_cnt_ + data_cnt_)), std::abs(100.0 * hits_inst_ / inst_cnt_), std::abs(100.0 * hits_data_ / data_cnt_));
}

class Proccesor {
private:
    uint32_t pc;
    std::vector<fragment>& frag_;
    std::vector<uint32_t> regs_;
    bool need_to_write_;

public:
    Proccesor(std::vector<fragment>& frag, const std::vector<uint32_t>& regs, bool write = true) : frag_(frag), need_to_write_(write) {
        regs_.resize(33);
        regs_[0] = 0;
        std::copy(regs.begin() + 1, regs.end(), regs_.begin() + 1);
        pc = regs[0];
    };

    template <CRP T>
    void StartProgramming(DataToWrite& data) {
        RAM ram(frag_);
        CacheController<T> cache;
        uint32_t ra = regs_[1];
        while (true) {
            if (pc == ra) {
                break;
            }
            uint32_t instr = cache.template ReadFromCache<uint32_t>(pc, false, ram);
            uint32_t opcode = GetOpcode(instr);
            uint32_t rd = GetRd(instr);
            uint32_t rs1 = GetRs1(instr);
            uint32_t rs2 = GetRs2(instr);
            uint32_t funct3 = GetFunct3(instr);
            uint32_t funct7 = GetFunct7(instr);
            if (opcode == 0b0110111) { // lui
                regs_[rd] = GetImmUType(instr);
                pc += 4;
            } else if (opcode == 0b0010111) { // auipc
                regs_[rd] = pc + GetImmUType(instr);
                pc += 4;
            } else if (opcode == 0b0010011) { // addi
                if (funct3 == 0b000) {
                    regs_[rd] = regs_[rs1] + GetImmIType(instr);
                    pc += 4;
                } else if (funct3 == 0b010) { // slti
                    if (static_cast<int32_t>(regs_[rs1]) < GetImmIType(instr)) {
                        regs_[rd] = 1;
                    } else {
                        regs_[rd] = 0;
                    }
                    pc += 4;
                } else if (funct3 == 0b011) { // sltiu
                    if (static_cast<uint32_t>(regs_[rs1]) < static_cast<uint32_t>(GetImmIType(instr))) {
                        regs_[rd] = 1;
                    } else {
                        regs_[rd] = 0;
                    }
                    pc += 4;
                } else if (funct3 == 0b100) { // xori
                    regs_[rd] = regs_[rs1] ^ GetImmIType(instr);
                    pc += 4;
                } else if (funct3 == 0b110) { // ori
                    regs_[rd] = regs_[rs1] | GetImmIType(instr);
                    pc += 4;
                } else if (funct3 == 0b111) { // andi
                    regs_[rd] = regs_[rs1] & GetImmIType(instr);
                    pc += 4;
                } else if (funct3 == 0b001 && funct7 == 0b0000000) { // slli
                    regs_[rd] = regs_[rs1] << GetShamt(instr);
                    pc += 4;
                } else if (funct3 == 0b101 && funct7 == 0b0000000) { // srli
                    regs_[rd] = regs_[rs1] >> GetShamt(instr);
                    pc += 4;
                } else if (funct3 == 0b101 && funct7 == 0b0100000) { // srai
                    regs_[rd] = static_cast<int32_t>(regs_[rs1]) >> GetShamt(instr);
                    pc += 4;
                }
            } else if (opcode == 0b0110011) {
                if (funct3 == 0b000 && funct7 == 0b0000000) { // add
                    regs_[rd] = regs_[rs1] + regs_[rs2];
                    pc += 4;
                } else if (funct3 == 0b000 && funct7 == 0b0100000) { // sub
                    regs_[rd] = regs_[rs1] - regs_[rs2];
                    pc += 4;
                } else if (funct3 == 0b001 && funct7 == 0b0000000) { // sll
                    regs_[rd] = regs_[rs1] << (regs_[rs2] & ((1UL << 5UL) - 1UL));
                    pc += 4;
                } else if (funct3 == 0b010 && funct7 == 0b0000000) { // slt
                    if (static_cast<int32_t>(regs_[rs1]) < static_cast<int32_t>(regs_[rs2]) ) {
                        regs_[rd] = 1;
                    } else {
                        regs_[rd] = 0;
                    }
                    pc += 4;
                } else if (funct3 == 0b011 && funct7 == 0b0000000) { // sltu
                    if (regs_[rs1] < regs_[rs2]) {
                        regs_[rd] = 1;
                    } else {
                        regs_[rd] = 0;
                    }
                    pc += 4;
                } else if (funct3 == 0b100 && funct7 == 0b0000000) { // xor
                    regs_[rd] = regs_[rs1] ^ regs_[rs2];
                    pc += 4;
                } else if (funct3 == 0b101 && funct7 == 0b0000000) { // srl
                    regs_[rd] = regs_[rs1] >> (regs_[rs2] & ((1UL << 5UL) - 1UL));
                    pc += 4;
                } else if (funct3 == 0b101 && funct7 == 0b0100000) { // sra
                    regs_[rd] = static_cast<int32_t>(regs_[rs1]) >> (regs_[rs2] & ((1UL << 5UL) - 1UL));
                    pc += 4;
                } else if (funct3 == 0b110 && funct7 == 0b0000000) { // or
                    regs_[rd] = regs_[rs1] | regs_[rs2];
                    pc += 4;
                }  else if (funct3 == 0b111 && funct7 == 0b0000000) { // and
                    regs_[rd] = regs_[rs1] & regs_[rs2];
                    pc += 4;
                } if (funct3 == 0b000 && funct7 == 0b0000001) { // mul
                    regs_[rd] = static_cast<int32_t>(regs_[rs1]) * static_cast<int32_t>(regs_[rs2]);
                    pc += 4;
                } else if (funct3 == 0b001 && funct7 == 0b0000001) { // mulh
                    regs_[rd] = (static_cast<int64_t>(regs_[rs1]) * static_cast<int64_t>(regs_[rs2])) >> 32;
                    pc += 4;
                } else if (funct3 == 0b010 && funct7 == 0b0000001) { // mulhsu
                    regs_[rd] = (static_cast<int64_t>(regs_[rs1]) * static_cast<uint64_t>(regs_[rs2])) >> 32;
                    pc += 4;
                } else if (funct3 == 0b011 && funct7 == 0b0000001) { // mulhu
                    regs_[rd] = (static_cast<uint64_t>(regs_[rs1]) * static_cast<uint64_t>(regs_[rs2])) >> 32;
                    pc += 4;
                } else if (funct3 == 0b100 && funct7 == 0b0000001) { // div
                    regs_[rd] = static_cast<int32_t>(regs_[rs1]) / static_cast<int32_t>(regs_[rs2]);
                    pc += 4;
                } else if (funct3 == 0b101 && funct7 == 0b0000001) { // divu
                    regs_[rd] = regs_[rs1] / regs_[rs2];
                    pc += 4;
                } else if (funct3 == 0b110 && funct7 == 0b0000001) { // rem
                    regs_[rd] = static_cast<int32_t>(regs_[rs1]) % static_cast<int32_t>(regs_[rs2]);
                    pc += 4;
                } else if (funct3 == 0b111 && funct7 == 0b0000001) { // remu
                    regs_[rd] = regs_[rs1] % regs_[rs2];
                    pc += 4;
                }
            } else if (opcode == 0b0010011 && funct3 == 0b000 && rd == 0 && rs1 == 0 && GetImmIType(instr) == 0) {
                pc += 4;
            } else if (opcode == 0b1101111)  { // jal
                regs_[rd] = pc + 4;
                pc += static_cast<int32_t>(GetImmJType(instr));
            } else if (opcode == 0b1100111) { // jalr
                uint32_t temp = pc + 4;
                pc = (regs_[rs1] + GetImmIType(instr)) & (~1);
                regs_[rd] = temp;
            } else if (opcode == 0b1100011) { // beq
                if (funct3 == 0b000) {
                    if (regs_[rs1] == regs_[rs2]) {
                        pc += static_cast<int32_t>(GetImmBType(instr));
                    } else {
                        pc += 4;
                    }
                } else if (funct3 == 0b001) { // bne
                    if (regs_[rs1] != regs_[rs2]) {
                        pc += static_cast<int32_t>(GetImmBType(instr));
                    } else {
                        pc += 4;
                    }
                }  else if (funct3 == 0b100) { // blt
                    if (static_cast<int32_t>(regs_[rs1]) < static_cast<int32_t>(regs_[rs2])) {
                        pc += static_cast<int32_t>(GetImmBType(instr));
                    } else {
                        pc += 4;
                    }
                } else if (funct3 == 0b101) { // bge
                    if (static_cast<int32_t>(regs_[rs1]) >= static_cast<int32_t>(regs_[rs2])) {
                        pc += static_cast<int32_t>(GetImmBType(instr));
                    } else {
                        pc += 4;
                    }
                } else if (funct3 == 0b110) { // bltu
                    if (regs_[rs1] < regs_[rs2]) {
                        pc += static_cast<int32_t>(GetImmBType(instr));
                    } else {
                        pc += 4;
                    }
                }  else if (funct3 == 0b111) { // bgeu
                    if (regs_[rs1] >= regs_[rs2]) {
                        pc += static_cast<int32_t>(GetImmBType(instr));
                    } else {
                        pc += 4;
                    }
                }
            } else if (opcode == 0b0000011) {
                if (funct3 == 0b000) { // lb
                    uint32_t addres = regs_[rs1] + GetImmIType(instr);
                    regs_[rd] = static_cast<int32_t>(cache.template ReadFromCache<uint8_t>(addres, true, ram));
                    pc += 4;
                } else if (funct3 == 0b001) { // lh
                    uint32_t addres = regs_[rs1] + GetImmIType(instr);
                    regs_[rd] = static_cast<int32_t>(cache.template ReadFromCache<uint16_t>(addres, true, ram));
                    pc += 4;
                } else if (funct3 == 0b010) { // lw
                    uint32_t addres = regs_[rs1] + GetImmIType(instr);
                    regs_[rd] = static_cast<int32_t>(cache.template ReadFromCache<uint32_t>(addres, true, ram));
                    pc += 4;
                } else if (funct3 == 0b100) { // lbu
                    uint32_t addres = regs_[rs1] + GetImmIType(instr);
                    regs_[rd] = cache.template ReadFromCache<uint8_t>(addres, true, ram);
                    pc += 4;
                }  else if (funct3 == 0b101) { // lhu
                    uint32_t addres = regs_[rs1] + GetImmIType(instr);
                    regs_[rd] = cache.template ReadFromCache<uint16_t>(addres, true, ram);
                    pc += 4;
                }
            } else if (opcode == 0b0100011) {
                if (funct3  == 0b000) { // sb
                    uint32_t addres = regs_[rs1] + GetImmSType(instr);
                    uint8_t value = regs_[rs2] & ((1UL << 8UL) - 1UL);
                    cache.template WriteInCache<uint8_t>(addres, true, value, ram);
                    pc += 4;
                } else if (funct3 == 0b001) { // sh
                    uint32_t addres = regs_[rs1] + GetImmSType(instr);
                    uint16_t value = regs_[rs2] & ((1UL << 16UL) - 1UL);
                    cache.template WriteInCache<uint16_t>(addres, true, value, ram);
                    pc += 4;
                }  else if (funct3 == 0b010) { // sh
                    uint32_t addres = regs_[rs1] + GetImmSType(instr);
                    cache.template WriteInCache<uint32_t>(addres, true, regs_[rs2], ram);
                    pc += 4;
                }
            } else if (instr == 0b00000000000000000000000001110011 || instr == 0b00000000000100000000000001110011) { // ecall or ebreak
                break;
            } else if (opcode == 0b0001111) { // fence(NOP)
                pc += 4; 
            }
        } 
        cache.PrintRate();
         if (need_to_write_) {
            cache.ClearCache(ram);
            data.buff = ram.GetData();
            data.regs.resize(32);
            data.regs[0] = pc;
            std::copy(regs_.begin() + 1, regs_.end(), data.regs.begin() + 1);
            FileWriter(data);
        }
    }
};

class Simulate {
public:
    Simulate(int argc, char* argv[]) : need_to_write(false), is_error(false) {
        Parser pr;
        Data data = pr.Parse(argc, argv); 
        BinParser bin_pr(data.filename1);
        frag_ = bin_pr.frag_ram_;
        regs_ = bin_pr.regs_;
        data_.addres = data.begin_addres;
        data_.len = data.size;
        data_.filename = data.filename2;
        if (data.filename2 != "") {
            need_to_write = true;
        }
        error = data.error_name;
        is_error = data.error;
    }

    void Start() {
        if (is_error) {
            std::cerr << error << std::endl;
        } else {
            printf("replacement\thit rate\thit rate (inst)\thit rate (data)\n");
            if (need_to_write) {
                Proccesor cpu(frag_, regs_);
                cpu.StartProgramming<CRP::LRU>(data_);
                Proccesor cpu2(frag_, regs_, false);
                cpu2.StartProgramming<CRP::pLRU>(data_);
            } else {
                Proccesor cpu(frag_, regs_, false);
                cpu.StartProgramming<CRP::LRU>(data_);
                Proccesor cpu2(frag_, regs_, false);
                cpu2.StartProgramming<CRP::pLRU>(data_);
            }
        }
    }

private:
    bool is_error;
    std::string error;
    bool need_to_write;
    DataToWrite data_;
    std::vector<fragment> frag_;
    std::vector<uint32_t> regs_;
};
};