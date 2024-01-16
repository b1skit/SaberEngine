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

		SEAssert("Invalid PBDataType", dataType != PBDataType::PBDataType_Count);
		m_platformParams->m_dataType = dataType;

		SEAssert("Invalid number of elements", 
			(numElements == 1 && dataType == PBDataType::SingleElement) || 
			(numElements >= 1 && dataType == PBDataType::Array));
		m_platformParams->m_numElements = numElements;
	}


	void ParameterBlock::RegisterAndCommit(
		std::shared_ptr<re::ParameterBlock> newPB, void const* data, uint32_t numBytes, uint64_t typeIDHash)
	{
		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.RegisterAndAllocateParameterBlock(newPB, numBytes);

		SEAssert("Invalid type detected. Can only set data of the original type",
			typeIDHash == newPB->m_typeIDHash);

		// Note: We commit via the PBM directly here, as we might be an immutable PB
		pbm.Commit(newPB->GetUniqueID(), data);

		// TODO: We should handle this internally; No point passing around PBs to another system
		RenderManager::Get()->RegisterForCreate(newPB); // Enroll for deferred platform layer creation
	}


	void ParameterBlock::CommitInternal(void const* data, uint64_t typeIDHash)
	{
		SEAssert("Invalid type detected. Can only set data of the original type",
			typeIDHash == m_typeIDHash);
		SEAssert("Cannot set data of an immutable param block", m_pbType == PBType::Mutable);

		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.Commit(GetUniqueID(), data);
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
		SEAssert("Parameter block destructor called, but parameter block is still marked as created. Did a parameter "
			"block go out of scope without Destroy() being called?", !params->m_isCreated);
	}


	void ParameterBlock::Destroy()
	{
		re::ParameterBlock::PlatformParams* params = GetPlatformParams();
		SEAssert("Parameter block has not been created, or has already been destroyed", params->m_isCreated);
		
		re::ParameterBlockAllocator& pbm = re::Context::Get()->GetParameterBlockAllocator();
		pbm.Deallocate(GetUniqueID()); // Internally makes a (deferred) call to platform::ParameterBlock::Destroy
	}
}