#include "seastar/core/do_with.hh"
#include "seastar/core/future.hh"
#include "seastar/core/app-template.hh"
#include "seastar/core/loop.hh"
#include "seastar/core/shared_ptr.hh"
#include "storage.h"

#include "native_thread_pool.h"
#include "v8.h"
#include "server.h"

#include <cstdlib>
#include <memory>

int main(int argc, char** argv) {
    seastar::app_template app;
    return app.run(argc, argv, [] {
        std::unique_ptr<v::ThreadPool> thread_pool_ptr = std::make_unique<v::ThreadPool>(1, seastar::smp::count, 1);
        return seastar::do_with(std::move(thread_pool_ptr), [](auto& thread_pool_ptr){
            return thread_pool_ptr->start()
            .then([&thread_pool_ptr](){
                std::unique_ptr<v8::Platform> platfrom_ptr = storage_t::init_v8();
                std::unique_ptr<v8_engine_server> server_ptr = std::make_unique<v8_engine_server>(*thread_pool_ptr);
                return seastar::do_with(std::move(platfrom_ptr), std::move(server_ptr), [](auto& platform, auto& server){

                    return server->start()
                    .then([&server]{
                        return server->listen();
                    })
                    .then([&server]{
                        return server->stop();
                    })
                    .then([](){
                        storage_t::shutdown_v8();
                        return seastar::make_ready_future<void>();
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
}