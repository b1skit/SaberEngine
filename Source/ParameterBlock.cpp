// � 2022 Adam Badke. All rights reserved.
#include "ParameterBlock.h"
#include "ParameterBlock_Platform.h"
#include "RenderManager.h"

using std::string;
using std::shared_ptr;
using std::make_shared;


namespace re
{
	// Private CTOR: Use one of the Create factories instead
	ParameterBlock::ParameterBlock(size_t typeIDHashCode, std::string const& pbName, PBType pbType)
		: NamedObject(pbName)
		, m_typeIDHash(typeIDHashCode)
		, m_pbType(pbType)
		, m_platformParams(nullptr)
	{
		platform::ParameterBlock::CreatePlatformParams(*this);
	}


	void ParameterBlock::RegisterAndCommit(
		std::shared_ptr<re::ParameterBlock> newPB, void const* data, size_t numBytes, uint64_t typeIDHash)
	{
		re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetContext().GetParameterBlockAllocator();
		pbm.RegisterAndAllocateParameterBlock(newPB, numBytes);

		SEAssert("Invalid type detected. Can only set data of the original type",
			typeIDHash == newPB->m_typeIDHash);

		// Note: We commit via the PBM directly here, as we might be an immutable PB
		pbm.Commit(newPB->GetUniqueID(), data);
	}


	void ParameterBlock::CommitInternal(void const* data, uint64_t typeIDHash)
	{
		SEAssert("Invalid type detected. Can only set data of the original type",
			typeIDHash == m_typeIDHash);
		SEAssert("Cannot set data of an immutable param block", m_pbType == PBType::Mutable);

		re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetContext().GetParameterBlockAllocator();
		pbm.Commit(GetUniqueID(), data);
	}


	void ParameterBlock::GetDataAndSize(void const*& out_data, size_t& out_numBytes) const
	{
		re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetContext().GetParameterBlockAllocator();
		pbm.GetDataAndSize(GetUniqueID(), out_data, out_numBytes);
	}


	size_t ParameterBlock::GetSize() const
	{
		re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetContext().GetParameterBlockAllocator();
		return pbm.GetSize(GetUniqueID());
	}


	void ParameterBlock::Destroy()
	{
		re::ParameterBlock::PlatformParams* params = GetPlatformParams()->As<re::ParameterBlock::PlatformParams*>();
		if (params->m_isCreated)
		{
			platform::ParameterBlock::Destroy(*this);

			re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetContext().GetParameterBlockAllocator();
			pbm.Deallocate(GetUniqueID());
		}
		else
		{
			LOG_ERROR("ParameterBlock::Destroy called for a parameter block that was not created");
		}
	}
}