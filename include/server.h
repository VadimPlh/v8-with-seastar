#pragma once

#include "seastar/core/future.hh"
#include "seastar/core/shared_ptr.hh"
#include "seastar/core/sstring.hh"
#include "seastar/core/thread.hh"
#include "seastar/http/common.hh"
#include "seastar/http/function_handlers.hh"
#include "seastar/http/handlers.hh"
#include "seastar/http/httpd.hh"
#include "seastar/http/reply.hh"

#include "storage.h"
#include <memory>

struct test_sum {
    int a;
    int b;

    int ans;
};


class v8_engine_server {
public:
    v8_engine_server(v::ThreadPool& thread_pool_)
    : scripts_storage(thread_pool_) {
        server = seastar::make_shared<seastar::httpd::http_server_control>();
    }

    seastar::future<> start() {
        return server->start()
        .then([this](){
            return server->set_routes([this](seastar::httpd::routes& r){
                set_routers(r);
            });
        });
    }

    seastar::future<> listen() {
        seastar::ipv4_addr ip_addr = {"127.0.0.1", port};
        auto server_addr = seastar::socket_address(ip_addr);
        return server->listen(server_addr);
    }

    seastar::future<> stop() {
        return server->stop();
    }

private:
    void set_routers(seastar::httpd::routes& r) {
        auto add_new_instanse_handler = new seastar::httpd::function_handler(
            [this](std::unique_ptr<seastar::httpd::request> req) -> seastar::future<seastar::json::json_return_type> {
                return scripts_storage.add_new_instance(req->get_query_param("name"), req->get_query_param("path"))
                .then([](auto res){
                    return seastar::make_ready_future<seastar::json::json_return_type>(res ? "OK" : "ERROR");
                });
            }
        );
        r.add(seastar::httpd::operation_type::POST, seastar::httpd::url("/add_new_instance"), add_new_instanse_handler);

        auto run_instance = new seastar::httpd::function_handler(
            [this](std::unique_ptr<seastar::httpd::request> req) -> seastar::future<seastar::json::json_return_type> {
                auto ptr = std::make_unique<test_sum>();
                ptr->a = std::atoi(req->get_query_param("a").data());
                ptr->b = std::atoi(req->get_query_param("b").data());
                std::span<char> data(reinterpret_cast<char*>(ptr.get()), sizeof(test_sum));
                return scripts_storage.run_instance(req->get_query_param("name"), data)
                .then([ptr = std::move(ptr)](auto res) mutable {
                    return seastar::make_ready_future<seastar::json::json_return_type>(res ? seastar::to_sstring(ptr->ans) : "ERROR");
                });
            }
        );
        r.add(seastar::httpd::operation_type::GET, seastar::httpd::url("/run_instance"), run_instance);

        auto delete_instance = new seastar::httpd::function_handler(
            [this](std::unique_ptr<seastar::httpd::request> req) -> seastar::future<seastar::json::json_return_type> {
                auto res = scripts_storage.delete_instance(req->get_query_param("name"));
                return seastar::make_ready_future<seastar::json::json_return_type>(res ? "OK" : "ERROR");
            }
        );
        r.add(seastar::httpd::operation_type::DELETE, seastar::httpd::url("/delete_instance"), delete_instance);
    }

    seastar::shared_ptr<seastar::httpd::http_server_control> server;
    const uint16_t port{12000};

    storage_t scripts_storage;
};