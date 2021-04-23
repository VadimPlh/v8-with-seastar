#include "seastar/core/do_with.hh"
#include "seastar/core/future.hh"
#include "seastar/core/app-template.hh"
#include "seastar/core/shared_ptr.hh"
#include "storage.h"

#include "native_thread_pool.h"
#include "v8.h"

#include <cstdlib>
#include <memory>


struct test_sum_t {
    int a;
    int b;

    int ans;
};

seastar::future<> run_simple(std::unique_ptr<storage_t>& storage_ptr) {
    auto* raw_ptr = new char[sizeof(test_sum_t)];
    storage_ptr->wrap_external_memory("simple_sum", raw_ptr, sizeof(test_sum_t));

    auto* obj_ptr = reinterpret_cast<test_sum_t*>(raw_ptr);
    obj_ptr->a = 1;
    obj_ptr->b = 3;
    obj_ptr->ans = 0;

    return storage_ptr->run_instance("simple_sum")
    .then([raw_ptr, obj_ptr](auto res){
        assert(obj_ptr->ans == obj_ptr->a + obj_ptr->b);
        delete[] raw_ptr;
        obj_ptr->~test_sum_t();
        return seastar::make_ready_future<void>();
    });
}

seastar::future<> run_wasm_simple(std::unique_ptr<storage_t>& storage_ptr) {
    auto* raw_ptr = new char[sizeof(test_sum_t)];
    storage_ptr->wrap_external_memory("sum_wasm", raw_ptr, sizeof(test_sum_t));

    auto* obj_ptr = reinterpret_cast<test_sum_t*>(raw_ptr);
    obj_ptr->a = 1;
    obj_ptr->b = 3;
    obj_ptr->ans = 0;

    return storage_ptr->run_instance("sum_wasm")
    .then([raw_ptr, obj_ptr](auto res){
        assert(obj_ptr->ans == obj_ptr->a + obj_ptr->b);
        delete[] raw_ptr;
        obj_ptr->~test_sum_t();
        return seastar::make_ready_future<void>();
    });
}

seastar::future<> run_loop(std::unique_ptr<storage_t>& storage_ptr) {
    auto* raw_ptr = new char[sizeof(int)];
    storage_ptr->wrap_external_memory("loop", raw_ptr, sizeof(int));
    return storage_ptr->run_instance("loop")
    .then([raw_ptr](auto res){
        delete[] raw_ptr;
        return seastar::make_ready_future<void>();
    });
}

int main(int argc, char** argv) {
    seastar::app_template app;
    return app.run(argc, argv, [] {
        std::unique_ptr<v::ThreadPool> thread_pool_ptr = std::make_unique<v::ThreadPool>(4, 4 * seastar::smp::count, 1);
        return seastar::do_with(std::move(thread_pool_ptr), [](auto& thread_pool_ptr){
            return thread_pool_ptr->start()
            .then([&thread_pool_ptr](){

                std::unique_ptr<v8::Platform> platfrom_ptr = storage_t::init_v8();
                return seastar::do_with(std::move(platfrom_ptr), [&thread_pool_ptr](auto& platform_ptr){

                    std::unique_ptr<storage_t> storage_ptr = std::make_unique<storage_t>(*thread_pool_ptr);
                    return seastar::do_with(std::move(storage_ptr), [](auto& storage_ptr){
                        return seastar::when_all(
                            storage_ptr->add_new_instance("loop", "/home/vadim/v8-with-seastar/examples/loop.js"),
                            storage_ptr->add_new_instance("simple_sum", "/home/vadim/v8-with-seastar/examples/simple.js"),
                            storage_ptr->add_new_instance("sum_wasm", "/home/vadim/v8-with-seastar/examples/sum_wasm.js")
                        ).discard_result()
                        .then([&storage_ptr](){
                            return seastar::when_all(
                                run_simple(storage_ptr),
                                run_wasm_simple(storage_ptr),
                                run_loop(storage_ptr)
                            ).discard_result();
                        })
                        .then([&storage_ptr](){
                            return seastar::when_all(
                                run_simple(storage_ptr),
                                run_wasm_simple(storage_ptr),
                                run_loop(storage_ptr)
                            ).discard_result();
                        })
                        .then([&storage_ptr](){
                            return seastar::when_all(
                                run_loop(storage_ptr),
                                run_loop(storage_ptr)
                            ).discard_result();
                        });
                    })

                    .then([](){
                        storage_t::shutdown_v8();
                        return seastar::make_ready_future<void>();
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