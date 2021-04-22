#pragma once

#include "native_thread_pool.h"
#include "v8-instance.h"

#include "libplatform/libplatform.h"
#include "v8.h"

#include "seastar/core/future.hh"
#include "seastar/core/when_all.hh"

#include <iostream>
#include <unordered_map>


class storage_t {
public:
    storage_t(v::ThreadPool& thread_pool_)
    : thread_pool(thread_pool_) {}

    seastar::future<bool> add_new_instance(const std::string& instance_name, const std::string& script_path) {
        auto engine_it = v8_instances.find(instance_name);
        if (engine_it != v8_instances.end()) {
            std::cout << "Script " << instance_name << "already exists" << std::endl; //TODO: use log system from seastar
            return seastar::make_ready_future<bool>(false);
        }

        return create_instance(instance_name, script_path);
    }

    seastar::future<bool> run_instance(std::string instance_name) {
        auto engine_it = v8_instances.find(instance_name);
        if (engine_it == v8_instances.end()) {
            std::cout << "Can not find script " << instance_name << std::endl;
            return seastar::make_ready_future<bool>(false);
        }

        auto run_future = thread_pool.submit([engine_it](){ engine_it->second.run_instance(); });
        auto stop_future = thread_pool.submit([engine_it](){ engine_it->second.stop_execution_loop(std::chrono::high_resolution_clock::now(), 3.0); });

        return seastar::when_all(std::move(run_future), std::move(stop_future))
        .then([engine_it](auto res){
            engine_it->second.continue_execution();
            return seastar::make_ready_future<bool>(true);
        });
    }

    bool delete_instance(const std::string& instance_name) {
        auto engine_it = v8_instances.find(instance_name);
        if (engine_it == v8_instances.end()) {
            std::cout << "Can not find script " << instance_name << std::endl;
            return false;
        }

        v8_instances.erase(engine_it);
        return true;
    }

    bool wrap_external_memory(const std::string& instance_name, char* data_ptr, size_t size) {
        auto engine_it = v8_instances.find(instance_name);
        if (engine_it == v8_instances.end()) {
            std::cout << "Can not find script " << instance_name << std::endl;
            return false;
        }

        engine_it->second.wrap_external_memory(data_ptr, size);
        return true;
    }

    static std::unique_ptr<v8::Platform> init_v8() {
        auto platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(platform.get());
        v8::V8::Initialize();
        return platform;
    }

    static void shutdown_v8() {
        v8::V8::Dispose();
        v8::V8::ShutdownPlatform();
    }

private:
    seastar::future<bool> create_instance(const std::string& instance_name, const std::string& script_path) {
        v8::Isolate::CreateParams create_params;
        create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();

        auto it = v8_instances.emplace(instance_name, std::move(create_params));
        return it.first->second.init_instance(script_path);
    }

    v::ThreadPool& thread_pool;
    std::unordered_map<std::string, v8_instance> v8_instances{};
};