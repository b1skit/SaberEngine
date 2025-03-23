// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "BufferView.h"
#include "VertexStream.h"


namespace core
{
	template<typename T>
	class InvPtr;
}


namespace re
{
	class IVertexStreamResourceSet : public virtual re::IBindlessResourceSet
	{
	public:
		IVertexStreamResourceSet(
			BindlessResourceManager* brm,
			char const* shaderName,
			uint32_t registerSpace,
			uint32_t baseOffset,
			uint32_t numResources,
			re::DataType streamDataType)
			: re::IBindlessResourceSet(brm, shaderName, registerSpace, baseOffset, numResources)
			, m_streamDataType(streamDataType)
		{
		}

		virtual ~IVertexStreamResourceSet() = 0;

	
	public: // IBindlessResourceSet:
		void PopulateRootSignatureDesc(void* dest) const override;


	public:
		re::DataType GetStreamDataType() const;


	private:
		re::DataType m_streamDataType;
	};
	inline IVertexStreamResourceSet::~IVertexStreamResourceSet() {} // Pure virtual: Must provide an impl


	inline re::DataType IVertexStreamResourceSet::GetStreamDataType() const
	{
		return m_streamDataType;
	}


	// ---


	template<typename T>
	class VertexStreamResourceSet : public virtual IVertexStreamResourceSet
	{
	public:
		VertexStreamResourceSet(
			re::BindlessResourceManager* brm,
			char const* shaderName,
			uint32_t registerSpace,
			uint32_t baseOffset,
			uint32_t numResources,
			re::DataType streamDataType)
			: IVertexStreamResourceSet(brm, shaderName, registerSpace, baseOffset, numResources, streamDataType)
			, re::IBindlessResourceSet(brm, shaderName, registerSpace, baseOffset, numResources)
		{
		}

		virtual ~VertexStreamResourceSet() override = default;
	};


	// ---


	struct IVertexStreamResource : public virtual IBindlessResource
	{
		IVertexStreamResource(VertexBufferInput const& vertexStream) : m_vertexBufferInput(vertexStream) {}

		virtual ~IVertexStreamResource() override = 0;


	public: // IBindlessResource:
		void GetPlatformResource(void* resourceOut, size_t resourceOutByteSize) override;
		void GetDescriptor(IBindlessResourceSet*, void* descriptorOut, size_t descriptorOutByteSize) override;


	public:
		template<typename T>
		static std::unique_ptr<IBindlessResourceSet> CreateBindlessResourceSetBase(
			re::BindlessResourceManager*,
			char const* shaderName,
			uint32_t baseOffset,
			uint32_t numResources,
			re::DataType streamDataType);


	public:
		static std::function<ResourceHandle(void)> GetRegistrationCallback(
			core::InvPtr<gr::VertexStream> const&);

		static std::function<void(ResourceHandle&)> GetUnregistrationCallback(gr::VertexStream::Type);


	public:
		VertexBufferInput m_vertexBufferInput;
	};
	inline IVertexStreamResource::~IVertexStreamResource() {}; // Pure virtual: Must provide an impl


	template<typename T>
	inline std::unique_ptr<IBindlessResourceSet> IVertexStreamResource::CreateBindlessResourceSetBase(
		re::BindlessResourceManager* brm,
		char const* shaderName,
		uint32_t baseOffset,
		uint32_t numResources,
		re::DataType streamDataType)
	{
		return std::make_unique<VertexStreamResourceSet<T>>(
			brm, shaderName, T::GetRegisterSpace(), baseOffset, numResources, streamDataType);
	}


	// ---


	// Use a macro to cut down on boilerplate: Defines "VertexStreamResource_<VertexStream::Type> structures
#define DEFINE_VERTEX_STREAM_RESOURCE(StreamType, DataType, RegisterSpace) \
struct VertexStreamResource_##StreamType \
	: public virtual IVertexStreamResource \
	{ \
		VertexStreamResource_##StreamType(VertexBufferInput const& vertexStream) \
			: IVertexStreamResource(vertexStream) {} \
\
		~VertexStreamResource_##StreamType() override = default;\
\
		static std::unique_ptr<IBindlessResourceSet> CreateBindlessResourceSet( \
			re::BindlessResourceManager* brm, uint32_t baseOffset, uint32_t numResources) \
		{ \
			return CreateBindlessResourceSetBase<VertexStreamResource_##StreamType>( \
				brm, "VertexStreams_"#StreamType, baseOffset, numResources, ##DataType); \
		} \
\
		static uint32_t GetRegisterSpace()\
		{\
			constexpr uint32_t k_registerSpace = ##RegisterSpace;\
			return k_registerSpace;\
		}\
	}; \


	// Finally, declare our final vertex stream resource types:
	DEFINE_VERTEX_STREAM_RESOURCE(Position, re::DataType::Float3, 10);
	DEFINE_VERTEX_STREAM_RESOURCE(Normal, re::DataType::Float3, 11);
	DEFINE_VERTEX_STREAM_RESOURCE(Tangent, re::DataType::Float4, 12);
	DEFINE_VERTEX_STREAM_RESOURCE(TexCoord, re::DataType::Float2, 13);
	DEFINE_VERTEX_STREAM_RESOURCE(Color, re::DataType::Float4, 14);

	DEFINE_VERTEX_STREAM_RESOURCE(Index, re::DataType::UInt, 15);
}
