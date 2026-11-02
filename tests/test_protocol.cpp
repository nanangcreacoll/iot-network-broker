#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>
#include <bit>
#include <gtest/gtest.h>

#include "core/protocol.hpp"

TEST(ProtocolParser, ValidPacket)
{
	using namespace iot::core;

    // Build a mock network packet in big-endian network byte order
    std::vector<std::byte> rawPacket(ProtocolHeaderSize + 5);
    
    ProtocolPacketHeader header{
        .magic = std::byteswap(ProtocolMagicHeader),
        .deviceId = std::byteswap(static_cast<uint64_t>(0x123456789ABCDEF0)),
        .sequenceId = std::byteswap(static_cast<uint32_t>(42)),
        .payloadLength = std::byteswap(static_cast<uint16_t>(5))
    };

    std::memcpy(rawPacket.data(), &header, ProtocolHeaderSize);
    std::memcpy(rawPacket.data() + ProtocolHeaderSize, "HELLO", 5);

    // Parse non-destructively without heap allocation
    auto view = ProtocolParser::parse(rawPacket);

    ASSERT_TRUE(view.has_value());
    ASSERT_EQ(view->deviceId, 0x123456789ABCDEF0);
    ASSERT_EQ(view->sequenceId, 42);
    ASSERT_EQ(view->payloadAsString(), "HELLO");
}
