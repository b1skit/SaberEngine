// © 2022 Adam Badke. All rights reserved.
#include "Context.h"
#include "ParameterBlock.h"
#include "ParameterBlock_Platform.h"
#include "RenderManager.h"

using std::string;
using std::shared_ptr;
using std::make_shared;


namespace re
{
	// Private CTOR: Use one of the Create factories instead
	ParameterBlock::ParameterBlock(
		size_t typeIDHashCode, std::string const& pbName, PBType pbType, PBDataType dataType, uint32_t numElements)
		: NamedObject(pbName)
		, m_typeIDHash(typeIDHashCode)
		, m_pbType(pbType)
		, m_platformParams(nullptr)
	{
		platform::ParameterBlock::CreatePlatformParams(*this);

		SEAssert(dataType != PBDataType::PBDataType_Count, "Invalid PBDataType");
		m_platformParams->m_dataType = dataType;

		SEAssert((numElements == 1 && dataType == PBDataType::SingleElement) || 
			(numElements >= 1 && dataType == PBDataType::Array),
			"Invalid number of elements");
		m_platformParams->m_numElements = numElements;
	}


	void ParameterBlock::Register(
		std::shared_ptr<re::ParameterBlock> newPB, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == newPB->m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");

		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.RegisterAndAllocateParameterBlock(newPB, numBytes);

		RenderManager::Get()->RegisterForCreate(newPB); // Enroll for deferred platform layer creation
	}


	void ParameterBlock::RegisterAndCommit(
		std::shared_ptr<re::ParameterBlock> newPB, void const* data, uint32_t numBytes, uint64_t typeIDHash)
	{
		Register(newPB, numBytes, typeIDHash);

		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.Commit(newPB->GetUniqueID(), data);

		newPB->m_platformParams->m_isCommitted = true;
	}


	void ParameterBlock::CommitInternal(void const* data, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");
		SEAssert(m_pbType == PBType::Mutable, "Cannot set data of an immutable param block");

		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.Commit(GetUniqueID(), data);
		
		m_platformParams->m_isCommitted = true;
	}


	void ParameterBlock::CommitInternal(void const* data, uint32_t dstBaseOffset, uint32_t numBytes, uint64_t typeIDHash)
	{
		SEAssert(typeIDHash == m_typeIDHash,
			"Invalid type detected. Can only set data of the original type");
		SEAssert(m_pbType == re::ParameterBlock::PBType::Mutable,
			"Only mutable parameter blocks can be partially updated");
		SEAssert(m_platformParams->m_dataType == PBDataType::Array,
			"Only array type parameter blocks can be partially updated");		

		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.Commit(GetUniqueID(), data, numBytes, dstBaseOffset);

		m_platformParams->m_isCommitted = true;
	}


	void ParameterBlock::GetDataAndSize(void const*& out_data, uint32_t& out_numBytes) const
	{
		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.GetDataAndSize(GetUniqueID(), out_data, out_numBytes);
	}


	uint32_t ParameterBlock::GetSize() const
	{
		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		return pbm.GetSize(GetUniqueID());
	}


	uint32_t ParameterBlock::GetStride() const
	{
		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		return pbm.GetSize(GetUniqueID()) / m_platformParams->m_numElements;
	}


	uint32_t ParameterBlock::GetNumElements() const
	{
		re::ParameterBlock::PlatformParams* params = GetPlatformParams();
		return params->m_numElements;
	}


	ParameterBlock::~ParameterBlock()
	{
		re::ParameterBlock::PlatformParams* params = GetPlatformParams();
		SEAssert(!params->m_isCreated,
			"Parameter block destructor called, but parameter block is still marked as created. Did a parameter "
			"block go out of scope without Destroy() being called?");
	}


	void ParameterBlock::Destroy()
	{
		re::ParameterBlock::PlatformParams* params = GetPlatformParams();
		SEAssert(params->m_isCreated, "Parameter block has not been created, or has already been destroyed");
		
		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.Deallocate(GetUniqueID()); // Internally makes a (deferred) call to platform::ParameterBlock::Destroy
	}
}