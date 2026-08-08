#include "netp2/upstream/connection_pool.h"

#include <cassert>
#include <iostream>
#include <memory>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>

using namespace netp2::upstream;

int main() {
    std::cout << "=== Connection Pool Unit Tests ===\n\n";

    try {
        std::cout << "Testing connection pool basic functionality...\n";

        boost::asio::io_context io(1);
        
        // 启动一个简单的 echo server
        boost::asio::ip::tcp::acceptor acceptor(io,
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
        const auto server_port = acceptor.local_endpoint().port();
        std::cout << "  Server listening on port " << server_port << "\n";

        bool test_completed = false;

        // 服务器任务：接受一个连接
        auto server_task = [&]() -> boost::asio::awaitable<void> {
            try {
                std::cout << "  Server: waiting for connection...\n";
                auto socket = co_await acceptor.async_accept(boost::asio::use_awaitable);
                std::cout << "  Server: accepted connection\n";
                // 保持连接打开
                co_await boost::asio::steady_timer(socket.get_executor(), 
                    std::chrono::milliseconds(500)).async_wait(boost::asio::use_awaitable);
                std::cout << "  Server: closing connection\n";
            } catch (const std::exception& ex) {
                std::cerr << "  Server error: " << ex.what() << "\n";
            }
        };

        // 客户端测试任务
        auto client_task = [&]() -> boost::asio::awaitable<void> {
            try {
                ConnectionPool pool(io);

                std::cout << "  Client: borrowing connection...\n";
                auto [conn1, error1] = co_await pool.borrow_connection("127.0.0.1", server_port);
                assert(error1 == UpstreamError::kSuccess);
                assert(conn1 != nullptr);
                std::cout << "  Client: connection established\n";

                // 测试归还连接
                pool.return_connection("127.0.0.1", server_port, std::move(conn1));
                std::cout << "  Client: connection returned to pool\n";

                test_completed = true;
            } catch (const std::exception& ex) {
                std::cerr << "  Client error: " << ex.what() << "\n";
            }
        };

        // 启动两个协程
        boost::asio::co_spawn(io, server_task(), boost::asio::detached);
        boost::asio::co_spawn(io, client_task(), boost::asio::detached);

        // 运行 io_context
        std::cout << "  Running io_context...\n";
        io.run();
        std::cout << "  io_context stopped\n";

        assert(test_completed && "Test did not complete");
        std::cout << "  PASSED\n";

        std::cout << "\n=== All tests PASSED ===\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception\n";
        return 1;
    }
}
