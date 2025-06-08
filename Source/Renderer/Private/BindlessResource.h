// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Private/BindlessResourceManager.h"
#include "Private/BufferView.h"
#include "Private/EnumTypes.h"

#include "Core/InvPtr.h"


namespace re
{
	class AccelerationStructure;
	class Buffer;
	class Texture;


	struct AccelerationStructureResource : public virtual IBindlessResource
	{
		AccelerationStructureResource(std::shared_ptr<re::AccelerationStructure> const& input)
			: m_resource(input)
		{
			m_viewType = re::ViewType::SRV;
		}

		~AccelerationStructureResource() override = default;


	public: // IBindlessResource:
		void GetPlatformResource(void* dest, size_t resourceOutByteSize) const override;
		void GetDescriptor(void* dest, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const override;
		void GetResourceUseState(void* dest, size_t destByteSize) const override;


	public:
		std::shared_ptr<re::AccelerationStructure> m_resource;
	};


	// --- 


	struct BufferResource : public virtual IBindlessResource
	{
		BufferResource(std::shared_ptr<re::Buffer> const& input, re::ViewType viewType = re::ViewType::SRV)
			: m_resource(input)
		{
			m_viewType = viewType;
		}

		~BufferResource() override = default;


	public: // IBindlessResource:
		void GetPlatformResource(void* dest, size_t resourceOutByteSize) const override;
		void GetDescriptor(void* dest, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const override;


	public:
		std::shared_ptr<re::Buffer> m_resource;
	};


	// --- 


	struct TextureResource : public virtual IBindlessResource
	{
		TextureResource(core::InvPtr<re::Texture> const& input, re::ViewType viewType = re::ViewType::SRV)
			: m_resource(input)
		{
			m_viewType = viewType;
		}

		~TextureResource() override = default;


	public: // IBindlessResource:
		void GetPlatformResource(void* dest, size_t resourceOutByteSize) const override;
		void GetDescriptor(void* dest, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const override;
		void GetResourceUseState(void* dest, size_t destByteSize) const override;


	public:
		core::InvPtr<re::Texture> m_resource;
	};


	// --- 


	struct VertexStreamResource : public virtual IBindlessResource
	{
		VertexStreamResource(VertexBufferInput const& input)
			: m_resource(input)
		{
			SEAssert(m_resource.GetBuffer() != nullptr, "Cannot add a resource with a null Buffer");
			m_viewType = re::ViewType::SRV;
		}

		~VertexStreamResource() override = default;


	public: // IBindlessResource:
		void GetPlatformResource(void* dest, size_t resourceOutByteSize) const override;
		void GetDescriptor(void* dest, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const override;


	public:
		VertexBufferInput m_resource;
	};
}