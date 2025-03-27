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
		IVertexStreamResourceSet(BindlessResourceManager* brm, char const* shaderName, re::DataType streamDataType)
			: re::IBindlessResourceSet(brm, shaderName)
			, m_streamDataType(streamDataType)
		{
		}

		virtual ~IVertexStreamResourceSet() = 0;

	
	public: // IBindlessResourceSet:
		void GetNullDescriptor(void* dest, size_t destByteSize) const override;
		void GetResourceUsageState(void* dest, size_t destByteSize) const override;


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
		VertexStreamResourceSet(re::BindlessResourceManager* brm, char const* shaderName, re::DataType streamDataType)
			: IVertexStreamResourceSet(brm, shaderName, streamDataType)
			, re::IBindlessResourceSet(brm, shaderName)
		{
		}

		virtual ~VertexStreamResourceSet() override = default;
	};


	// ---


	struct IVertexStreamResource : public virtual IBindlessResource
	{
		IVertexStreamResource(VertexBufferInput const& vertexStream)
			: m_vertexBufferInput(vertexStream) {}

		virtual ~IVertexStreamResource() override = 0;


	public: // IBindlessResource:
		void GetPlatformResource(void* resourceOut, size_t resourceOutByteSize) override;
		void GetDescriptor(IBindlessResourceSet*, void* descriptorOut, size_t descriptorOutByteSize) override;


	public:
		template<typename T>
		static std::unique_ptr<IBindlessResourceSet> CreateBindlessResourceSetBase(
			re::BindlessResourceManager*,
			char const* shaderName,
			re::DataType streamDataType);


	public:
		static std::function<ResourceHandle(void)> GetRegistrationCallback(
			core::InvPtr<gr::VertexStream> const&);

		static std::function<void(ResourceHandle&)> GetUnregistrationCallback(re::DataType);

		static ResourceHandle GetResourceHandle(VertexBufferInput const&);
		static ResourceHandle GetResourceHandle(core::InvPtr<gr::VertexStream> const&);


	public:
		VertexBufferInput m_vertexBufferInput;
	};
	inline IVertexStreamResource::~IVertexStreamResource() {}; // Pure virtual: Must provide an impl


	template<typename T>
	inline std::unique_ptr<IBindlessResourceSet> IVertexStreamResource::CreateBindlessResourceSetBase(
		re::BindlessResourceManager* brm,
		char const* shaderName,
		re::DataType streamDataType)
	{
		return std::make_unique<VertexStreamResourceSet<T>>(brm, shaderName, streamDataType);
	}


	// ---


	// Use a macro to cut down on boilerplate: Defines "VertexStreamResource_<DataTypeName> structures
#define DEFINE_VERTEX_STREAM_RESOURCE(DataTypeName) \
struct VertexStreamResource_##DataTypeName \
	: public virtual IVertexStreamResource \
	{ \
		VertexStreamResource_##DataTypeName(VertexBufferInput const& vertexStream) \
			: IVertexStreamResource(vertexStream) {} \
\
		~VertexStreamResource_##DataTypeName() override = default;\
\
		static std::unique_ptr<IBindlessResourceSet> CreateBindlessResourceSet(re::BindlessResourceManager* brm) \
		{ \
			return CreateBindlessResourceSetBase<VertexStreamResource_##DataTypeName>( \
				brm, "VertexStreams_"#DataTypeName, re::DataType::##DataTypeName); \
		} \
	}; \
	
	// Finally, declare our final vertex stream resource types:
	DEFINE_VERTEX_STREAM_RESOURCE(Float2);
	DEFINE_VERTEX_STREAM_RESOURCE(Float3);
	DEFINE_VERTEX_STREAM_RESOURCE(Float4);
	DEFINE_VERTEX_STREAM_RESOURCE(UShort);
	DEFINE_VERTEX_STREAM_RESOURCE(UInt);
}
