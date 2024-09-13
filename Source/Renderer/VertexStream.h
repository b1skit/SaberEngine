// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/IPlatformParams.h"

#include "Core/Util/ByteVector.h"


namespace re
{
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

		enum class DataType : uint8_t // Of each component in a vertex stream element. Eg. Color/Float4 == Float
		{
			Float,	// 32-bit
			Float2,
			Float3,
			Float4,

			Int,	// 32-bit
			Int2,
			Int3,
			Int4,

			UInt,	// 32-bit
			UInt2,
			UInt3,
			UInt4,

			Short,	// 16-bit
			Short2,
			Short4,

			UShort,	// 16-bit
			UShort2,
			UShort4,

			Byte,	// 8-bit
			Byte2,
			Byte4,

			UByte,	// 8-bit
			UByte2,
			UByte4,

			DataType_Count
		};

		static constexpr char const* DataTypeToCStr(DataType);
		static constexpr uint8_t DataTypeToNumComponents(DataType);
		static constexpr uint8_t DataTypeToComponentByteSize(DataType);
		

		enum class Type : uint8_t
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

		enum class IsMorphData : bool
		{
			False,
			True
		};

		enum class Lifetime : bool
		{
			SingleFrame,
			Permanent,
		};

		struct CreateParams
		{
			Lifetime m_lifetime = Lifetime::Permanent;

			Type m_type = Type::Type_Count;

			IsMorphData m_isMorphData = IsMorphData::False;

			DataType m_dataType = DataType::DataType_Count;
			Normalize m_doNormalize = Normalize::False;
		};


	public:
		[[nodiscard]] static std::shared_ptr<re::VertexStream> Create(CreateParams const&, util::ByteVector&&);

		VertexStream(VertexStream&&) = default;
		VertexStream& operator=(VertexStream&&) = default;

		~VertexStream() { Destroy(); };

		Lifetime GetLifetime() const;

		Type GetType() const;

		bool IsMorphData() const;

		DataType GetDataType() const; // What data type does each individual component have?
		Normalize DoNormalize() const; // Should the data be normalized when it is accessed by the GPU?

		void const* GetData() const;
		util::ByteVector& GetDataByteVector();
		util::ByteVector const& GetDataByteVector() const;

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

		util::ByteVector m_data;

		std::unique_ptr<PlatformParams> m_platformParams;


	private: // Use the Create() factory instead
		VertexStream(CreateParams const&, util::ByteVector&& data);


	private: // No copying allowed
		VertexStream(VertexStream const&) = delete;
		VertexStream& operator=(VertexStream const&) = delete;
	};


	inline VertexStream::Lifetime VertexStream::GetLifetime() const
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


	inline util::ByteVector& VertexStream::GetDataByteVector()
	{
		return m_data;
	}


	inline util::ByteVector const& VertexStream::GetDataByteVector() const
	{
		return m_data;
	}


	constexpr char const* VertexStream::DataTypeToCStr(DataType dataType)
	{
		switch (dataType)
		{
		case re::VertexStream::DataType::Float: return "Float";
		case re::VertexStream::DataType::Float2: return "Float2";
		case re::VertexStream::DataType::Float3: return "Float3";
		case re::VertexStream::DataType::Float4: return "Float4";

		case re::VertexStream::DataType::Int: return "Int";
		case re::VertexStream::DataType::Int2: return "Int2";
		case re::VertexStream::DataType::Int3: return "Int3";
		case re::VertexStream::DataType::Int4: return "Int4";

		case re::VertexStream::DataType::UInt: return "UInt";
		case re::VertexStream::DataType::UInt2: return "UInt2";
		case re::VertexStream::DataType::UInt3: return "UInt3";
		case re::VertexStream::DataType::UInt4: return "UInt4";

		case re::VertexStream::DataType::Short: return "Short";
		case re::VertexStream::DataType::Short2: return "Short2";
		case re::VertexStream::DataType::Short4: return "Short4";

		case re::VertexStream::DataType::UShort: return "UShort";
		case re::VertexStream::DataType::UShort2: return "UShort2";
		case re::VertexStream::DataType::UShort4: return "UShort4";

		case re::VertexStream::DataType::Byte: return "Byte";
		case re::VertexStream::DataType::Byte2: return "Byte2";
		case re::VertexStream::DataType::Byte4: return "Byte4";

		case re::VertexStream::DataType::UByte: return "UByte";
		case re::VertexStream::DataType::UByte2: return "UByte2";
		case re::VertexStream::DataType::UByte4: return "UByte4";
		default: return "INVALID_DATA_TYPE";
		}
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline re::VertexStream::PlatformParams::~PlatformParams() {};
}