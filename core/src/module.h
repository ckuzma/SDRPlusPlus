#pragma once
#include <string>
#include <map>
#include <json.hpp>

#ifdef _WIN32
#ifdef SDRPP_IS_CORE
#define SDRPP_EXPORT extern "C" __declspec(dllexport)
#else
#define SDRPP_EXPORT extern "C" __declspec(dllimport)
#endif
#else
#define SDRPP_EXPORT extern
#endif

#ifdef _WIN32
#include <Windows.h>
#define MOD_EXPORT extern "C" __declspec(dllexport)
#else
#include <dlfcn.h>
#define MOD_EXPORT extern "C"
#endif

namespace mod {

    struct Module_t {
#ifdef _WIN32
        HINSTANCE inst;
#else
        void* inst;
#endif
        void (*_INIT_)();
        void* (*_CREATE_INSTANCE_)(std::string name);
        void (*_DELETE_INSTANCE_)(void* instance);
        void (*_STOP_)();
        void* ctx;
    };

    struct ModuleInfo_t {
        const char* name;
        const char* description;
        const char* author;
        const char* version;
    };

    void loadModule(std::string path, std::string name);
    void loadFromList(std::string path);
    bool isLoaded(void* handle);
    
    extern std::map<std::string, Module_t> modules;
    extern std::vector<std::string> moduleNames;
};

#define MOD_INFO    MOD_EXPORT const mod::ModuleInfo_t _INFO