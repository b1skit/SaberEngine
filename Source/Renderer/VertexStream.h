// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "EnumTypes.h"

#include "Core/Interfaces/IHashedDataObject.h"

#include "Core/Util/ByteVector.h"


namespace core
{
	template<typename T>
	class InvPtr;
}
namespace dx12
{
	class RenderManager;
}
namespace opengl
{
	class RenderManager;
}
namespace re
{
	class Buffer;
	struct VertexStreamView;
}

namespace gr
{
	class VertexStream : public virtual core::IHashedDataObject
	{
	public:
		static constexpr uint8_t k_maxVertexStreams = 16;


	public:
		enum Normalize : bool
		{
			False = 0,
			True = 1
		};


		enum Type : uint8_t
		{
			Position,
			Normal,
			//Binormal,
			Tangent,
			TexCoord,
			Color,
			BlendIndices,	// Joints
			BlendWeight,
			//PointSize, // Note: Point size is not (currently) supported as OpenGL has no equivalent

			Index,

			Type_Count
		};
		static constexpr char const* TypeToCStr(Type);


	public:
		struct StreamDesc
		{
			re::Lifetime m_lifetime = re::Lifetime::Permanent;

			Type m_type = Type::Type_Count;

			re::DataType m_dataType = re::DataType::DataType_Count; // Per component in each element. Eg. Color/Float4 == Float
			Normalize m_doNormalize = Normalize::False;
		};


	public:
		struct MorphData
		{
			std::unique_ptr<util::ByteVector> m_displacementData;
			re::DataType m_dataType;
		};
		struct CreateParams
		{
			std::unique_ptr<util::ByteVector> m_streamData;
			gr::VertexStream::StreamDesc m_streamDesc{};
			uint8_t m_setIdx = std::numeric_limits<uint8_t>::max();

			std::vector<MorphData> m_morphTargetData; // 1 entry per displacement

			// TODO: Should this be part of the data hash (and if so, moved to the StreamDesc)?
			re::Buffer::UsageMask m_extraUsageBits = 0; // Logically OR'd with our default vertex/index flags
		};


	public:
		[[nodiscard]] static core::InvPtr<gr::VertexStream> Create(
			StreamDesc const&, util::ByteVector&&, re::Buffer::UsageMask extraUsageBits = 0);

		[[nodiscard]] static core::InvPtr<gr::VertexStream> Create(CreateParams&&);

		VertexStream(VertexStream&&) noexcept = default;
		VertexStream& operator=(VertexStream&&) noexcept = default;

		~VertexStream();

		void Destroy();

		re::Lifetime GetLifetime() const;

		Type GetType() const;

		re::DataType GetDataType() const; // What data type does each individual component have?
		bool DoNormalize() const; // Should the data be normalized when it is accessed by the GPU?

		uint32_t GetTotalDataByteSize() const;
		uint32_t GetNumElements() const; // How many vertices-worth of attributes do we have?

		re::Buffer const* GetBuffer() const;
		std::shared_ptr<re::Buffer> const& GetBufferSharedPtr() const;


	public:
		void ShowImGuiWindow() const;


	protected:
		void ComputeDataHash() override;
		

	protected:
		friend class dx12::RenderManager;
		friend class opengl::RenderManager;
		void CreateBuffers(core::InvPtr<gr::VertexStream> const&);


	private:
		StreamDesc m_streamDesc;

		std::shared_ptr<re::Buffer> m_streamBuffer;
		
		// Vertex streams are often loaded asyncronously. To prevent race conditions around Buffer 
		// registration/allocation/committing, we temporarily store everything we need to create the Buffer, and then
		// immediately release it after creation
		struct DeferredBufferCreateParams
		{
			util::ByteVector m_data;
			re::Buffer::UsageMask m_extraUsageBits;
		};
		std::unique_ptr<DeferredBufferCreateParams> m_deferredBufferCreateParams;


	private: // Use the Create() factory instead
		VertexStream(StreamDesc const&, util::ByteVector&& data, util::HashKey, re::Buffer::UsageMask extraUsageBits);


	private: // No copying allowed
		VertexStream(VertexStream const&) = delete;
		VertexStream& operator=(VertexStream const&) = delete;
	};


	inline re::Lifetime VertexStream::GetLifetime() const
	{
		return m_streamDesc.m_lifetime;
	}


	inline VertexStream::Type VertexStream::GetType() const
	{
		return m_streamDesc.m_type;
	}


	inline re::Buffer const* VertexStream::GetBuffer() const
	{
		return m_streamBuffer.get();
	}


	inline std::shared_ptr<re::Buffer> const& VertexStream::GetBufferSharedPtr() const
	{
		return m_streamBuffer;
	}


	constexpr char const* VertexStream::TypeToCStr(Type streamType)
	{
		switch (streamType)
		{
		case Type::Position: return "Position";
		case Type::Normal: return "Normal";
		//case Type::Binormal: return "Binormal";
		case Type::Tangent: return "Tangent";
		case Type::TexCoord: return "TexCoord";
		case Type::Color: return "Color";
		case Type::BlendIndices: return "BlendIndices";
		case Type::BlendWeight: return "BlendWeight";
		case Type::Index: return "Index";
		default: return "INVALID_VERTEX_STREAM_TYPE_ENUM_RECEIVED";
		}
		SEStaticAssert(gr::VertexStream::Type_Count == 8, "Number of vertex stream types changed. This must be updated");
	}
}