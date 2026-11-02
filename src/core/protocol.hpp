#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <optional>

namespace iot::core
{
	constexpr std::size_t ProtocolHeaderSize = 18;
	constexpr uint32_t ProtocolMagicHeader = 0xDEADBEEF;

#pragma pack(push, 1)
	/**
	 * @brief Protocol packet header structure.
	 */
	struct ProtocolPacketHeader
	{
		uint32_t magic;
		uint64_t deviceId;
		uint32_t sequenceId;
		uint16_t payloadLength;
	};
#pragma pack(pop)

	/**
	 * @brief Zero-copy Protocol packet view referencing raw socket memory.
	 */
	struct ProtocolPacketView
	{
		uint64_t deviceId{0};
		uint32_t sequenceId{0};
		std::span<const std::byte> payload;

		[[nodiscard]] std::string_view payloadAsString() const noexcept
		{
			return std::string_view(reinterpret_cast<const char *>(payload.data()), payload.size());
		}
	};

	class ProtocolParser
	{
	public:
		/**
		 * @brief Parses a binary frame in zero-copy mode from a memory buffer.
		 * @param buffer Non-owning view over incoming packet memory.
		 * @return std::optional<ProtocolPacketView> non-nullopt if valid frame found.
		 */
		[[nodiscard]] static std::optional<ProtocolPacketView> parse(const std::span<const std::byte> &buffer) noexcept
		{
			if (buffer.size() < ProtocolHeaderSize)
			{
				return std::nullopt; // Not enough data for header
			}

			const auto *header = reinterpret_cast<const ProtocolPacketHeader *>(buffer.data());

			// Byte-order swapping from network (big-endian) to host endianness
			const uint32_t magic = std::byteswap(header->magic);
			if (magic != ProtocolMagicHeader)
			{
				return std::nullopt; // Invalid magic header
			}

			const uint64_t payloadLength = std::byteswap(header->payloadLength);
			if (buffer.size() < ProtocolHeaderSize + payloadLength)
			{
				return std::nullopt; // Not enough data for payload
			}

			return ProtocolPacketView{
				.deviceId = std::byteswap(header->deviceId),
				.sequenceId = std::byteswap(header->sequenceId),
				.payload = buffer.subspan(ProtocolHeaderSize, payloadLength)
			};
		}
	};
	
} // namespace iot::core