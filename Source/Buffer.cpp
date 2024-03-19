// © 2022 Adam Badke. All rights reserved.
#include "Context.h"
#include "Buffer.h"
#include "Buffer_Platform.h"
#include "RenderManager.h"


namespace re
{
	// Private CTOR: Use one of the Create factories instead
	Buffer::Buffer(
		size_t typeIDHashCode, std::string const& bufferName, BufferParams const& bufferParams, uint32_t dataByteSize)
		: NamedObject(bufferName)
		, m_typeIDHash(typeIDHashCode)
		, m_dataByteSize(dataByteSize)
		, m_bufferParams(bufferParams)
		, m_platformParams(nullptr)
	{
		SEAssert(m_bufferParams.m_type != Type::Type_Count,
			"Invalid Type");
		SEAssert(m_bufferParams.m_dataType != DataType::DataType_Count, "Invalid DataType");
		SEAssert((m_bufferParams.m_dataType == DataType::Constant && m_bufferParams.m_numElements == 1) || 
			(m_bufferParams.m_dataType == DataType::Structured && m_bufferParams.m_numElements >= 1),
			"Invalid number of elements");
		
		platform::Buffer::CreatePlatformParams(*this);
	}


	void Buffer::Register(
		std::shared_ptr<re::Buffer> newBuffer, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == newBuffer->m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");

		re::BufferAllocator& ba = re::Context::Get()->GetBufferAllocator();
		ba.RegisterAndAllocateBuffer(newBuffer, numBytes);

		RenderManager::Get()->RegisterForCreate(newBuffer); // Enroll for deferred platform layer creation
	}


	void Buffer::RegisterAndCommit(
		std::shared_ptr<re::Buffer> newBuffer, void const* data, uint32_t numBytes, uint64_t typeIDHash)
	{
		Register(newBuffer, numBytes, typeIDHash);

		re::BufferAllocator& ba = re::Context::Get()->GetBufferAllocator();
		ba.Commit(newBuffer->GetUniqueID(), data);

		newBuffer->m_platformParams->m_isCommitted = true;
	}


	void Buffer::CommitInternal(void const* data, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");
		SEAssert(m_bufferParams.m_type == Type::Mutable, "Cannot set data of an immutable buffer");

		re::BufferAllocator& ba = re::Context::Get()->GetBufferAllocator();
		ba.Commit(GetUniqueID(), data);
		
		m_platformParams->m_isCommitted = true;
	}


	void Buffer::CommitInternal(void const* data, uint32_t dstBaseOffset, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");
		SEAssert(m_bufferParams.m_type == re::Buffer::Type::Mutable,
			"Only mutable buffers can be partially updated");
		SEAssert(m_bufferParams.m_dataType == DataType::Structured,
			"Only structured buffers can be partially updated");

		re::BufferAllocator& ba = re::Context::Get()->GetBufferAllocator();
		ba.Commit(GetUniqueID(), data, numBytes, dstBaseOffset);

		m_platformParams->m_isCommitted = true;
	}


	void Buffer::GetDataAndSize(void const** out_data, uint32_t* out_numBytes) const
	{
		re::BufferAllocator& ba = re::Context::Get()->GetBufferAllocator();
		ba.GetData(GetUniqueID(), out_data);

		*out_numBytes = m_dataByteSize;
	}


	Buffer::~Buffer()
	{
		re::Buffer::PlatformParams* params = GetPlatformParams();
		SEAssert(!params->m_isCreated,
			"Buffer destructor called, but buffer is still marked as created. Did a parameter "
			"block go out of scope without Destroy() being called?");
	}


	void Buffer::Destroy()
	{
		re::Buffer::PlatformParams* params = GetPlatformParams();
		SEAssert(params->m_isCreated, "Buffer has not been created, or has already been destroyed");
		
		re::BufferAllocator& ba = re::Context::Get()->GetBufferAllocator();
		ba.Deallocate(GetUniqueID()); // Internally makes a (deferred) call to platform::Buffer::Destroy
	}
}