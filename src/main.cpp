#include <iostream>
#include <chrono>
#include <thread>
#include "core/spsc_queue.hpp"
#include "core/protocol.hpp"
#include "network/tcp_server.hpp"

int main()
{
  std::cout << "Starting IoT Network Broker..." << std::endl;

  constexpr std::size_t QueueSize = 1024;
  constexpr int Port = 8080;

  iot::core::SPSCQueue<std::vector<std::byte>, QueueSize> networkToBrokerQueue;
  iot::network::TCPServer server(Port, networkToBrokerQueue);
  
  server.start();
  std::cout << "TCP Server started on port " << Port << "." << std::endl;
  
  std::atomic<bool> running{true};
  std::thread brokerThread([&]() {
    uint64_t processedPackets = 0;

    while (running)
    {
      auto data = networkToBrokerQueue.pop();
      if (!data)
      {
        std::this_thread::yield();
        continue;
      }

      auto packet = iot::core::ProtocolParser::parse(*data);
      if (packet)
      {
        processedPackets++;
        std::cout << "Processed Packet #" << processedPackets
                  << " from Device ID: 0x" << std::hex << packet->deviceId
                  << " payload: " << packet->payloadAsString() << std::dec << std::endl;
      }
    }
    std::cout << "Broker thread processed " << processedPackets << " total packets." << std::endl;
  });

  std::this_thread::sleep_for(std::chrono::seconds(30));
  running = false;
  brokerThread.join();
  server.stop();

  std::cout << "IoT Network Broker stopped." << std::endl;

  return 0;
}
