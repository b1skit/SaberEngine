// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "VertexStream.h"


namespace re
{
	class VertexStreamMap final
	{
	public:
		VertexStreamMap();

		~VertexStreamMap() = default;
		VertexStreamMap(VertexStreamMap const&) = default;
		VertexStreamMap(VertexStreamMap&&) noexcept = default;
		VertexStreamMap& operator=(VertexStreamMap const&) = default;
		VertexStreamMap& operator=(VertexStreamMap&&) noexcept = default;

		bool operator==(VertexStreamMap const&);

	public:
		static constexpr uint8_t k_invalidSlotIdx = std::numeric_limits<uint8_t>::max();

		uint8_t GetSlotIdx(gr::VertexStream::Type, uint8_t semanticIdx) const;
		void SetSlotIdx(gr::VertexStream::Type, uint8_t semanticIdx, re::DataType, uint8_t slotIdx);

		uint8_t GetNumSlots() const;

		struct StreamMetadata;
		StreamMetadata const* GetStreamMetadata(uint8_t& arraySizeOut) const;


	public:
		struct VertexStreamKey final
		{
			gr::VertexStream::Type m_streamType; // Name portion of the semantic: E.g. NORMAL0 -> Type::Normal
			uint8_t m_semanticIdx;	// Numeric part of the semantic. E.g. NORMAL0 -> 0
		};
		struct StreamMetadata final
		{
			VertexStreamKey m_streamKey;
			re::DataType m_streamDataType;

			uint8_t m_shaderSlotIdx;
		};


	private:
		uint8_t m_numAttributes;

		std::array<StreamMetadata, gr::VertexStream::k_maxVertexStreams> m_slotLayout; // Sorted by m_streamKey


		struct StreamMetadataComparator final
		{
			inline bool operator()(StreamMetadata const& a, StreamMetadata const& b)
			{
				if (a.m_streamKey.m_streamType == b.m_streamKey.m_streamType)
				{
					return a.m_streamKey.m_semanticIdx < b.m_streamKey.m_semanticIdx;
				}
				return a.m_streamKey.m_streamType < b.m_streamKey.m_streamType;
			}


			inline bool operator()(StreamMetadata const& a, VertexStreamKey const& b)
			{
				if (a.m_streamKey.m_streamType == b.m_streamType)
				{
					return a.m_streamKey.m_semanticIdx < b.m_semanticIdx;
				}
				return a.m_streamKey.m_streamType < b.m_streamType;
			}


			inline bool operator()(VertexStreamKey const& a, StreamMetadata const& b)
			{
				if (a.m_streamType == b.m_streamKey.m_streamType)
				{
					return a.m_semanticIdx < b.m_streamKey.m_semanticIdx;
				}
				return a.m_streamType < b.m_streamKey.m_streamType;
			}
		};


		void ValidateSlotIndexes();
	};


	inline VertexStreamMap::VertexStreamMap()
		: m_numAttributes(0)
	{
		memset(m_slotLayout.data(), 0, sizeof(StreamMetadata) * m_slotLayout.size());
	}


	inline bool VertexStreamMap::operator==(VertexStreamMap const& rhs)
	{
		if (m_numAttributes != rhs.m_numAttributes)
		{
			return false;
		}
		for (uint8_t i = 0; i < m_numAttributes; ++i)
		{
			if (m_slotLayout[i].m_streamKey.m_streamType != rhs.m_slotLayout[i].m_streamKey.m_streamType ||
				m_slotLayout[i].m_streamKey.m_semanticIdx != rhs.m_slotLayout[i].m_streamKey.m_semanticIdx ||
				m_slotLayout[i].m_streamDataType != rhs.m_slotLayout[i].m_streamDataType ||
				m_slotLayout[i].m_shaderSlotIdx != rhs.m_slotLayout[i].m_shaderSlotIdx)
			{
				return false;
			}
		}
		return true;
	}


	inline uint8_t VertexStreamMap::GetSlotIdx(gr::VertexStream::Type streamType, uint8_t semanticIdx) const
	{
		auto result = std::lower_bound( // Find 1st element >=
			m_slotLayout.begin(),
			m_slotLayout.begin() + m_numAttributes,
			VertexStreamKey{ streamType, semanticIdx },
			StreamMetadataComparator());

		if (result != m_slotLayout.end() &&
			result->m_streamKey.m_streamType == streamType &&
			result->m_streamKey.m_semanticIdx == semanticIdx)
		{
			return result->m_shaderSlotIdx;
		}
		return k_invalidSlotIdx;
	}


	inline void VertexStreamMap::SetSlotIdx(
		gr::VertexStream::Type streamType,
		uint8_t semanticIdx,
		re::DataType dataType,
		uint8_t slotIdx)
	{
		SEAssert(m_numAttributes <= gr::VertexStream::k_maxVertexStreams, "Vertex stream map is full");
		SEAssert(semanticIdx < gr::VertexStream::k_maxVertexStreams &&
			slotIdx < gr::VertexStream::k_maxVertexStreams,
			"OOB index received");

		auto result = std::lower_bound( // Find 1st element >=
			m_slotLayout.begin(),
			m_slotLayout.begin() + m_numAttributes,
			re::VertexStreamMap::VertexStreamKey{
				.m_streamType = streamType,
				.m_semanticIdx = semanticIdx },
			StreamMetadataComparator());

		SEAssert(result != m_slotLayout.end(), "Could not find an insertion point for the vertex stream");

		SEAssert(result->m_streamKey.m_streamType != streamType ||
			result->m_streamKey.m_semanticIdx != semanticIdx ||
			m_numAttributes == 0,
			"Found stream type/semantic index collision");

		const size_t insertIdx = result - m_slotLayout.begin();
		for (uint8_t i = m_numAttributes; i > 0 && i > insertIdx; --i)
		{
				SEAssert(m_slotLayout[i - 1].m_streamKey.m_streamType != 
						m_slotLayout[i].m_streamKey.m_streamType ||
					m_slotLayout[i - 1].m_streamKey.m_semanticIdx < 
						m_slotLayout[i].m_streamKey.m_semanticIdx,
					"Found stream type/semantic index collision");

			m_slotLayout[i] = m_slotLayout[i - 1];
		}

		m_slotLayout[insertIdx] = StreamMetadata{
			.m_streamKey{
				.m_streamType = streamType,
				.m_semanticIdx = semanticIdx,
			},
			.m_streamDataType = dataType,
			.m_shaderSlotIdx = slotIdx,
		};

		m_numAttributes++;

		ValidateSlotIndexes(); // _DEBUG only
	}


	inline uint8_t VertexStreamMap::GetNumSlots() const
	{
		return m_numAttributes;
	}


	inline void VertexStreamMap::ValidateSlotIndexes()
	{
#if defined(_DEBUG)
		std::unordered_set<uint8_t> seenSlots;
		seenSlots.reserve(m_slotLayout.size());
		for (uint8_t i = 0; i < m_numAttributes; ++i)
		{
			SEAssert(!seenSlots.contains(m_slotLayout[i].m_shaderSlotIdx),
				"Found a colliding shader attribute slot");
			seenSlots.insert(m_slotLayout[i].m_shaderSlotIdx);			
		}
#endif
	}


	inline VertexStreamMap::StreamMetadata const* VertexStreamMap::GetStreamMetadata(uint8_t& arrSizeOut) const
	{
		arrSizeOut = m_numAttributes;
		return m_slotLayout.data();
	}
}