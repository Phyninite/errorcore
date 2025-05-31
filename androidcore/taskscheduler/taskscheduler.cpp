#include "pch.h"
#include "logs.h"
#include "Update.hpp"
#include "thirdparty-includes/Hook/And64InlineHook.hpp"

#include "pch.h"
#include "logs.h"
#include "Update.hpp"
#include "thirdparty-includes/Hook/And64InlineHook.hpp"

#define whsj "WaitingHybridScriptsJob"

void* original_job_start = nullptr;
int64_t job_list[10];
int job_list_index = 0;

int64_t hooked_job_start(int64_t a1) {
    auto retrieved = a1;
    
    job_list[job_list_index % 10] = retrieved;
    job_list_index++;
    
    if (retrieved == (int64_t)whsj) {
        auto rLenc = retrieved + 512;
        LOGD("whsj: 0x%llx", retrieved);
    }
    LOGD("WaitingHybridScriptsJob: 0x%llx", retrieved);
    ((int64_t(*)(int64_t))original_job_start)(a1);
    return retrieved;
}

void init_task_scheduler() {
    A64HookFunction(Addresses::job_start, (void*)hooked_job_start, &original_job_start);
}
