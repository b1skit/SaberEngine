// © 2025 Adam Badke. All rights reserved.
#include "BindlessResource_VertexStream.h"
#include "BindlessResource_VertexStream_Platform.h"
#include "Context.h"
#include "RenderManager.h"

#include "Core/InvPtr.h"


namespace re
{
	void IVertexStreamResourceSet::GetNullDescriptor(void* dest, size_t destByteSize) const
	{
		platform::VertexStreamResourceSet::GetNullDescriptor(*this, dest, destByteSize);
	}


	void IVertexStreamResourceSet::GetResourceUsageState(void* dest, size_t destByteSize) const
	{
		platform::VertexStreamResourceSet::GetResourceUsageState(*this, dest, destByteSize);
	}


	// ---


	void IVertexStreamResource::GetPlatformResource(void* resourceOut, size_t resourceOutByteSize)
	{
		return platform::IVertexStreamResource::GetPlatformResource(*this, resourceOut, resourceOutByteSize);
	}


	void IVertexStreamResource::GetDescriptor(
		IBindlessResourceSet* resourceSet, void* descriptorOut, size_t descriptorOutByteSize)
	{
		return platform::IVertexStreamResource::GetDescriptor(*resourceSet, *this, descriptorOut, descriptorOutByteSize);
	}


	std::function<ResourceHandle(void)> IVertexStreamResource::GetRegistrationCallback(
		core::InvPtr<gr::VertexStream> const& vertexStream)
	{
		// Note: We intentionally capture the vertexBufferInput by value here
		auto RegisterVertexStream = [vertexStream]() -> ResourceHandle
			{
				re::Context* context = re::Context::Get();

				re::BindlessResourceManager* brm = context->GetBindlessResourceManager();
				SEAssert(brm, "Failed to get BindlessResourceManager");

				switch (vertexStream->GetDataType())
				{
				case re::DataType::Float2:
				{
					return brm->RegisterResource<re::VertexStreamResource_Float2>(
						std::make_unique<re::VertexStreamResource_Float2>(vertexStream));
				}
				break;
				case re::DataType::Float3:
				{
					return brm->RegisterResource<re::VertexStreamResource_Float3>(
						std::make_unique<re::VertexStreamResource_Float3>(vertexStream));
				}
				break;
				case re::DataType::Float4:
				{
					return brm->RegisterResource<re::VertexStreamResource_Float4>(
						std::make_unique<re::VertexStreamResource_Float4>(vertexStream));
				}
				break;
				case re::DataType::UShort:
				{
					return brm->RegisterResource<re::VertexStreamResource_UShort>(
						std::make_unique<re::VertexStreamResource_UShort>(vertexStream));
				}
				break;
				case re::DataType::UInt:
				{
					return brm->RegisterResource<re::VertexStreamResource_UInt>(
						std::make_unique<re::VertexStreamResource_UInt>(vertexStream));
				}
				break;
				default: SEAssertF("Data type is not currently supported");
				}
				return k_invalidResourceHandle; // This should never happen
				
				SEStaticAssert(static_cast<uint8_t>(re::DataType::DataType_Count) == 24,
					"Data type count has changed. This must be updated");
			};

		return RegisterVertexStream;
	}


	std::function<void(ResourceHandle&)> IVertexStreamResource::GetUnregistrationCallback(
		re::DataType dataType)
	{
		auto UnregisterVertexStream = [dataType](ResourceHandle& resourceHandle)
			{
				re::Context* context = re::Context::Get();

				re::BindlessResourceManager* brm = context->GetBindlessResourceManager();
				SEAssert(brm, "Failed to get BindlessResourceManager");

				const uint64_t frameNum = re::RenderManager::Get()->GetCurrentRenderFrameNum();

				switch (dataType)
				{
				case re::DataType::Float2:
				{
					 brm->UnregisterResource<re::VertexStreamResource_Float2>(resourceHandle, frameNum);
				}
				break;
				case re::DataType::Float3:
				{
					 brm->UnregisterResource<re::VertexStreamResource_Float3>(resourceHandle, frameNum);
				}
				break;
				case re::DataType::Float4:
				{
					 brm->UnregisterResource<re::VertexStreamResource_Float4>(resourceHandle, frameNum);
				}
				break;
				case re::DataType::UShort:
				{
					 brm->UnregisterResource<re::VertexStreamResource_UShort>(resourceHandle, frameNum);
				}
				break;
				case re::DataType::UInt:
				{
					 brm->UnregisterResource<re::VertexStreamResource_UInt>(resourceHandle, frameNum);
				}
				break;
				default: SEAssertF("Data type is not currently supported");
				}
				SEStaticAssert(static_cast<uint8_t>(re::DataType::DataType_Count) == 24,
					"Data type count has changed. This must be updated");
			};

		return UnregisterVertexStream;
	}


	ResourceHandle IVertexStreamResource::GetResourceHandle(VertexBufferInput const& vertexBufferInput)
	{
		SEAssert(vertexBufferInput.GetStream().IsValid() &&
			vertexBufferInput.GetBuffer() &&
			vertexBufferInput.GetBuffer()->GetBindlessResourceHandle() != k_invalidResourceHandle,
			"Vertex stream is not valid for use as a bindless resource");

		return vertexBufferInput.GetBuffer()->GetBindlessResourceHandle();
	}


	ResourceHandle IVertexStreamResource::GetResourceHandle(core::InvPtr<gr::VertexStream> const& vertexStream)
	{
		SEAssert(vertexStream.IsValid() &&
			vertexStream->GetBuffer() &&
			vertexStream->GetBuffer()->GetBindlessResourceHandle() != k_invalidResourceHandle,
			"Vertex stream is not valid for use as a bindless resource");

		return vertexStream->GetBuffer()->GetBindlessResourceHandle();
	}
}