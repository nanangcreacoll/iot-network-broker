#pragma once

#include <cstddef>
#include <cstdint>
#include <thread>
#include "core/spsc_queue.hpp"

namespace iot::network
{
	class TCPServer
	{
	public:
		TCPServer(uint16_t port, iot::core::SPSCQueue<std::vector<std::byte>, 1024> &queue);
		~TCPServer();

		TCPServer(const TCPServer &) = delete;
		TCPServer &operator=(const TCPServer &) = delete;

		void start();
		void stop();
	
	private:
		uint16_t serverPort;
		int serverFd{-1};
		int epollFd{-1};
		std::atomic<bool> running{false};
		std::thread serverThread;
		iot::core::SPSCQueue<std::vector<std::byte>, 1024> &que;

		void eventLoop();
		bool setNonBlocking(int fd);
		void handleAccept();
		void handleRead(int clientFd);
	};
}