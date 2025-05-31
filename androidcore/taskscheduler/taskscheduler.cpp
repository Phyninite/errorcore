#include "pch.h"
#include "logs.h"
#include "Update.hpp"
#include "../memory/memory.hpp"
#include "../thirdparty-includes/Hook/And64InlineHook.hpp"

#ifdef __aarch64__
#error "This code is for 32-bit ARM only"
#endif

#define whsj "WaitingHybridScriptsJob"

void* original_job_start = nullptr;
uintptr_t job_list[10];
int job_list_index = 0;

uintptr_t hooked_job_start(uintptr_t a1) {
    auto retrieved = a1;
    
    job_list[job_list_index % 10] = retrieved;
    job_list_index++;
    
    if (strcmp((char*)retrieved, whsj) == 0) {
        auto rLenc = retrieved + 512;
        LOGD("whsj: 0x%x", retrieved);
    }
    LOGD("WaitingHybridScriptsJob: 0x%x", retrieved);
    ((uintptr_t(*)(uintptr_t))original_job_start)(a1);
    return retrieved;
}

void init_task_scheduler() {
#ifdef __aarch64__
    *((int*)0) = 0;
#endif
    A64HookFunction((void*)memory::rebaseAddress(Addresses::job_start), (void*)hooked_job_start, &original_job_start);
}
