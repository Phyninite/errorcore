#include <thread>
#include <chrono>
#include <android/log.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "luacode.h"
}

#define LOG_TAG "androidcore"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

extern "C" int execyield(lua_State* exploitLstate) {
    if (lua_gettop(exploitLstate) != 3) {
        LOGD("execyield: Incorrect number of arguments. Expected 3. Got %d.", lua_gettop(exploitLstate));
        lua_pushinteger(exploitLstate, LUA_ERRERR);
        lua_pushnil(exploitLstate);
        return 2;
    }

    if (!lua_isstring(exploitLstate, 1)) {
        LOGD("execyield: Argument 1 (bytecode) must be a string.");
        lua_pushinteger(exploitLstate, LUA_ERRERR);
        lua_pushnil(exploitLstate);
        return 2;
    }
    if (!lua_isstring(exploitLstate, 2)) {
        LOGD("execyield: Argument 2 (chunkName) must be a string.");
        lua_pushinteger(exploitLstate, LUA_ERRERR);
        lua_pushnil(exploitLstate);
        return 2;
    }
    if (!lua_isnumber(exploitLstate, 3)) {
        LOGD("execyield: Argument 3 (waitTimeMs) must be an integer.");
        lua_pushinteger(exploitLstate, LUA_ERRERR);
        lua_pushnil(exploitLstate);
        return 2;
    }

    size_t bytecode_size;
    const char* bytecode_data = lua_tolstring(exploitLstate, 1, &bytecode_size);
    const char* chunk_name = lua_tostring(exploitLstate, 2);
    int wait_time_ms = lua_tointeger(exploitLstate, 3);

    if (wait_time_ms > 0) {
        LOGD("execyield: Waiting for %dms before executing chunk '%s'.", wait_time_ms, chunk_name);
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
        LOGD("execyield: Wait finished for chunk '%s'.", chunk_name);
    }

    lua_State* L_coroutine = lua_newthread(exploitLstate);
    if (!L_coroutine) {
        LOGD("execyield: Failed to create new Lua thread (coroutine) for chunk '%s'.", chunk_name);
        lua_pushinteger(exploitLstate, LUA_ERRMEM);
        lua_pushnil(exploitLstate);
        return 2;
    }
    int coroutine_thread_idx_on_main_stack = lua_absindex(exploitLstate, -1);

    LOGD("execyield: Loading bytecode for chunk '%s' into new coroutine.", chunk_name);
    int load_status = luau_load(L_coroutine, chunk_name, bytecode_data, bytecode_size, 0);

    if (load_status != LUA_OK) {
        const char* error_msg = lua_tostring(L_coroutine, -1);
        LOGD("execyield: Error loading bytecode for chunk '%s': %s", chunk_name, (error_msg ? error_msg : "Unknown error"));
        lua_remove(exploitLstate, coroutine_thread_idx_on_main_stack);
        lua_pushinteger(exploitLstate, load_status);
        lua_pushnil(exploitLstate);
        return 2;
    }
    LOGD("execyield: Bytecode loaded successfully for chunk '%s'.", chunk_name);

    LOGD("execyield: Attempting to resume coroutine for chunk '%s'.", chunk_name);
    int resume_status = lua_resume(L_coroutine, nullptr, 0);

    if (resume_status == LUA_OK) {
        LOGD("execyield: Coroutine for chunk '%s' finished execution successfully.", chunk_name);
        lua_pushinteger(exploitLstate, resume_status);
        lua_pushvalue(exploitLstate, coroutine_thread_idx_on_main_stack);
        return 2;
    } else if (resume_status == LUA_YIELD) {
        LOGD("execyield: Coroutine for chunk '%s' yielded.", chunk_name);
        lua_pushinteger(exploitLstate, resume_status);
        lua_pushvalue(exploitLstate, coroutine_thread_idx_on_main_stack);
        return 2;
    } else {
        const char* error_msg = lua_tostring(L_coroutine, -1);
        LOGD("execyield: Error during coroutine execution for chunk '%s': %s", chunk_name, (error_msg ? error_msg : "Unknown error"));
        lua_remove(exploitLstate, coroutine_thread_idx_on_main_stack);
        lua_pushinteger(exploitLstate, resume_status);
        lua_pushnil(exploitLstate);
        return 2;
    }
}
