#include "seastar/core/do_with.hh"
#include "seastar/core/future.hh"
#include "seastar/core/app-template.hh"
#include "storage.h"

#include <memory>

struct test_sum_t {
    int a;
    int b;

    int ans;
};

int main(int argc, char** argv) {
    seastar::app_template app;
    return app.run(argc, argv, [] {
        auto platform_ptr = storage_t::init_v8();
        std::unique_ptr<storage_t> storage_ptr = std::make_unique<storage_t>();
        return seastar::do_with(
        std::move(platform_ptr),
        std::move(storage_ptr),
        [](auto& platform, auto& storage_ptr){
            return seastar::do_with(
                std::move(storage_ptr),
                [](auto& storage_ptr ) mutable {
                    return storage_ptr->add_new_instance("test", "/home/vadim/v8-with-seastar/examples/simple.js")
                    .then([&storage_ptr](auto result){
                        auto* raw_ptr = new char[sizeof(test_sum_t)];
                        storage_ptr->wrap_external_memory("test", raw_ptr, sizeof(test_sum_t));

                        auto* obj_ptr = reinterpret_cast<test_sum_t*>(raw_ptr);
                        obj_ptr->a = 5;
                        obj_ptr->b = 7;
                        obj_ptr->ans = 0;

                        storage_ptr->run_instance("test");

                        assert(obj_ptr->ans == obj_ptr->a + obj_ptr->b);
                        obj_ptr->~test_sum_t();

                        delete[] raw_ptr;

                        return seastar::make_ready_future<void>();
                    });
                }
            )
            .then([](){
                storage_t::shutdown_v8();
                return seastar::make_ready_future<int>(0);
            });
        });
    });
}