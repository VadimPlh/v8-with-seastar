#include "seastar/core/do_with.hh"
#include "seastar/core/future.hh"
#include "seastar/core/app-template.hh"
#include "seastar/core/shared_ptr.hh"
#include "storage.h"

#include "native_thread_pool.h"

#include <memory>

struct test_sum_t {
    int a;
    int b;

    int ans;
};

int main(int argc, char** argv) {
    seastar::app_template app;
    return app.run(argc, argv, [] {
        std::unique_ptr<v::ThreadPool> thread_pool_ptr = std::make_unique<v::ThreadPool>(3, 2 * seastar::smp::count, 1);
        return seastar::do_with(std::move(thread_pool_ptr), [](auto& thread_pool_ptr){
            return thread_pool_ptr->start()
            .then([&thread_pool_ptr]() mutable {
                auto platform_ptr = storage_t::init_v8();
                std::unique_ptr<storage_t> storage_ptr = std::make_unique<storage_t>(*thread_pool_ptr);
                return seastar::do_with(
                std::move(platform_ptr),
                std::move(storage_ptr),
                [](auto& platform, auto& storage_ptr){
                    return seastar::do_with(
                        std::move(storage_ptr),
                        [](auto& storage_ptr ) mutable {
                        return storage_ptr->add_new_instance("test", "/home/vadim/v8-with-seastar/examples/loop.js")
                        .then([&storage_ptr](auto result){
                            auto* raw_ptr = new char[sizeof(test_sum_t)];
                            storage_ptr->wrap_external_memory("test", raw_ptr, sizeof(test_sum_t));

                            auto* obj_ptr = reinterpret_cast<test_sum_t*>(raw_ptr);

                            return storage_ptr->run_instance("test")
                            .then([&storage_ptr, obj_ptr, raw_ptr](bool call_result){
                                delete[] raw_ptr;

                                return seastar::make_ready_future<void>();
                            });
                        });
                    });
                });
            })
            .then([&thread_pool_ptr]() mutable {
                return thread_pool_ptr->stop();
            })
            .then([](){
                return seastar::make_ready_future<int>(0);
            });
        });
    });
}