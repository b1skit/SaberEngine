// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "EnumTypes.h"

#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/IPlatformParams.h"

#include "Core/Util/ByteVector.h"


namespace re
{
	class Buffer;


	class VertexStream : public virtual core::IHashedDataObject
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
		};


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
			Binormal,
			Tangent,
			TexCoord,
			Color,
			BlendIndices,
			BlendWeight,
			//PointSize, // Note: Point size is not (currently) supported as OpenGL has no equivalent

			Index,

			Type_Count
		};
		static constexpr char const* TypeToCStr(Type);


		enum class IsMorphData : bool
		{
			False,
			True
		};

		struct CreateParams
		{
			re::Lifetime m_lifetime = re::Lifetime::Permanent;

			Type m_type = Type::Type_Count;

			IsMorphData m_isMorphData = IsMorphData::False;

			re::DataType m_dataType = DataType::DataType_Count; // Per component in each element. Eg. Color/Float4 == Float
			Normalize m_doNormalize = Normalize::False;
		};


	public:
		[[nodiscard]] static std::shared_ptr<re::VertexStream> Create(CreateParams const&, util::ByteVector&&);

		VertexStream(VertexStream&&) = default;
		VertexStream& operator=(VertexStream&&) = default;

		~VertexStream() { Destroy(); };

		re::Lifetime GetLifetime() const;

		Type GetType() const;

		bool IsMorphData() const;

		DataType GetDataType() const; // What data type does each individual component have?
		Normalize DoNormalize() const; // Should the data be normalized when it is accessed by the GPU?

		re::Buffer const* GetBuffer() const;

		uint32_t GetTotalDataByteSize() const;
		
		uint8_t GetNumComponents() const; // Number of individual components per element. i.e. 1/2/3/4 (only)
		
		uint32_t GetNumElements() const; // How many vertices-worth of attributes do we have?
		uint8_t GetElementByteSize() const; // Total number of bytes for a single element (ie. all components)

		PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }


	public:
		void ShowImGuiWindow() const;


	protected:
		void ComputeDataHash() override;


	private:
		void Destroy();
		

	private:
		CreateParams m_createParams;

		std::shared_ptr<re::Buffer> m_streamBuffer;

		std::unique_ptr<PlatformParams> m_platformParams;


	private: // Use the Create() factory instead
		VertexStream(CreateParams const&, util::ByteVector&& data);


	private: // No copying allowed
		VertexStream(VertexStream const&) = delete;
		VertexStream& operator=(VertexStream const&) = delete;
	};


	inline re::Lifetime VertexStream::GetLifetime() const
	{
		return m_createParams.m_lifetime;
	}


	inline VertexStream::Type VertexStream::GetType() const
	{
		return m_createParams.m_type;
	}


	inline bool VertexStream::IsMorphData() const
	{
		return m_createParams.m_isMorphData == IsMorphData::True;
	}


	inline re::Buffer const* VertexStream::GetBuffer() const
	{
		return m_streamBuffer.get();
	}


	constexpr char const* VertexStream::TypeToCStr(Type streamType)
	{
		switch (streamType)
		{
		case Type::Position: return "Position";
		case Type::Normal: return "Normal";
		case Type::Binormal: return "Binormal";
		case Type::Tangent: return "Tangent";
		case Type::TexCoord: return "TexCoord";
		case Type::Color: return "Color";
		case Type::BlendIndices: return "BlendIndices";
		case Type::BlendWeight: return "BlendWeight";
		case Type::Index: return "Index";
		default: return "INVALID_VERTEX_STREAM_TYPE_ENUM_RECEIVED";
		}
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline re::VertexStream::PlatformParams::~PlatformParams() {};
}