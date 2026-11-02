#include "tcp_server.hpp"

#include <vector>
#include <span>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>

namespace iot::network
{
	constexpr int MaxEvents = 64;
	constexpr std::size_t ReadBufferSize = 4096;
	constexpr int EpollTimeoutMs = 100;

	TCPServer::TCPServer(uint16_t port, iot::core::SPSCQueue<std::vector<std::byte>, 1024> &queue)
		: serverPort(port), que(queue) {}

	TCPServer::~TCPServer()
	{
		this->stop();
	}

	bool TCPServer::setNonBlocking(int fd)
	{
		auto flags = fcntl(fd, F_GETFL, 0);
		if (flags == -1) return false;
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
	}

	void TCPServer::start()
	{
		this->serverFd = socket(AF_INET, SOCK_STREAM, 0);
		if (this->serverFd < 0)
		{
			throw std::runtime_error("Failed to create socket");
		}

		auto opt = 1;
		setsockopt(this->serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(this->serverPort);

		if (bind(this->serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
		{
			throw std::runtime_error("Failed to bind socket");
		}

		if (listen(this->serverFd, SOMAXCONN) < 0)
		{
			throw std::runtime_error("Failed to listen on socket");
		}

		this->setNonBlocking(this->serverFd);

		this->epollFd = epoll_create1(0);
		if (this->epollFd < 0)
		{
			throw std::runtime_error("Failed to create epoll instance");
		}

		epoll_event event{};
		event.events = EPOLLIN | EPOLLET; // Edge-triggered non-blocking socket
		event.data.fd = this->serverFd;
		epoll_ctl(this->epollFd, EPOLL_CTL_ADD, this->serverFd, &event);

		this->running.store(true);
		this->serverThread = std::thread(&TCPServer::eventLoop, this);
	}

	void TCPServer::eventLoop()
	{
		std::vector<epoll_event> events(MaxEvents);

		while (this->running.load())
		{
			auto nfds = epoll_wait(this->epollFd, events.data(), MaxEvents, EpollTimeoutMs);
			for (int i = 0; i < nfds; ++i)
			{
				if (events[i].data.fd == this->serverFd)
				{
					this->handleAccept();
				}
				else
				{
					this->handleRead(events[i].data.fd);
				}
			}
		}
	}

	void TCPServer::handleAccept()
	{
		while (true)
		{
			sockaddr_in clientAddr{};
			socklen_t clientLen = sizeof(clientAddr);
			auto clientFd = accept(this->serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
			if (clientFd < 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK) break;
				break;
			}

			this->setNonBlocking(clientFd);
			epoll_event event{};
			event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
			event.data.fd = clientFd;
			epoll_ctl(this->epollFd, EPOLL_CTL_ADD, clientFd, &event);
		}
	}

	void TCPServer::handleRead(int clientFd)
	{
		std::vector<std::byte> buffer(ReadBufferSize);
		while (true)
		{
			auto bytesRead = read(clientFd, buffer.data(), buffer.size());
			if (bytesRead < 0)
			{
				buffer.resize(static_cast<std::size_t>(bytesRead));
				while (!this->que.push(buffer)) std::this_thread::yield();
			}
			else if (bytesRead == 0 || (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
			{
				epoll_ctl(this->epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
				close(clientFd);
				break;
			}
			else
			{
				break;
			}
		}
	}

	void TCPServer::stop()
	{
		if (this->running.exchange(false))
		{
			if (this->serverThread.joinable()) this->serverThread.join();
			if (this->epollFd >= 0) close(this->epollFd);
			if (this->serverFd >= 0) close(this->serverFd);
		}
	}
}