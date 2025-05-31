#include "X32Hook.hpp"
#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
// 32 bit hooking library by me.
static inline uint32_t get_page_size() {
    static uint32_t page_size = 0;
    if (page_size == 0) {
        page_size = static_cast<uint32_t>(sysconf(_SC_PAGESIZE));
    }
    return page_size;
}

static inline void* get_page_start(void* addr) {
    uint32_t page_size = get_page_size();
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & ~(page_size - 1));
}

static inline int make_executable(void* addr, size_t len) {
    void* page_start = get_page_start(addr);
    uint32_t page_size = get_page_size();
    size_t page_len = ((reinterpret_cast<uintptr_t>(addr) + len - reinterpret_cast<uintptr_t>(page_start) + page_size - 1) & ~(page_size - 1));
    
    return mprotect(page_start, page_len, PROT_READ | PROT_WRITE | PROT_EXEC);
}

int X32Hook(void* target, void* replacement) {
    if (!target || !replacement) {
        return -1;
    }
    
    uint32_t* target_addr = static_cast<uint32_t*>(target);
    uint32_t target_uint = reinterpret_cast<uint32_t>(target);
    uint32_t replacement_uint = reinterpret_cast<uint32_t>(replacement);
    
    if (make_executable(target_addr, 12) != 0) {
        return -2;
    }
    
    bool is_thumb = (target_uint & 1);
    if (is_thumb) {
        target_addr = reinterpret_cast<uint32_t*>(target_uint & ~1);
        uint16_t* thumb_addr = reinterpret_cast<uint16_t*>(target_addr);
        
        int32_t offset = static_cast<int32_t>((replacement_uint | 1) - (reinterpret_cast<uint32_t>(thumb_addr) + 4));
        
        if (offset >= -16777216 && offset <= 16777214 && (offset & 1) == 0) {
            offset >>= 1;
            uint16_t s = (offset >> 24) & 1;
            uint16_t i1 = (offset >> 23) & 1;
            uint16_t i2 = (offset >> 22) & 1;
            uint16_t imm10 = (offset >> 12) & 0x3FF;
            uint16_t imm11 = (offset >> 1) & 0x7FF;
            uint16_t j1 = (~(i1 ^ s)) & 1;
            uint16_t j2 = (~(i2 ^ s)) & 1;
            
            thumb_addr[0] = 0xF000 | (s << 10) | imm10;
            thumb_addr[1] = 0xB800 | (j1 << 13) | (j2 << 11) | imm11;
        } else {
            thumb_addr[0] = 0x4778;
            thumb_addr[1] = 0x46C0;
            target_addr[1] = replacement_uint | 1;
        }
    } else {
        int32_t offset = static_cast<int32_t>(replacement_uint - (reinterpret_cast<uint32_t>(target_addr) + 8));
        
        if (offset >= -33554432 && offset <= 33554428 && (offset & 3) == 0) {
            uint32_t branch_instr = 0xEA000000 | ((offset >> 2) & 0x00FFFFFF);
            *target_addr = branch_instr;
        } else {
            target_addr[0] = 0xE51FF004;
            target_addr[1] = replacement_uint;
        }
    }
    
    __builtin___clear_cache(reinterpret_cast<char*>(target_addr), reinterpret_cast<char*>(target_addr) + 12);
    
    return 0;
}
