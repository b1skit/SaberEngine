#include "ParameterBlock.h"
#include "ParameterBlock_Platform.h"
#include "RenderManager.h"

using std::string;
using std::shared_ptr;
using std::make_shared;


namespace re
{
	// Pseudo-private CTOR: private ParameterBlock::Accessor forces access via one of the Create factories
	ParameterBlock::ParameterBlock(
		ParameterBlock::Accessor, size_t typeIDHashCode, std::string pbName, UpdateType updateType, Lifetime lifetime)
		: NamedObject(pbName)
		, m_typeIDHash(typeIDHashCode)
		, m_lifetime(lifetime)
		, m_updateType(updateType)
		, m_isDirty(true)
		, m_platformParams(nullptr)
	{
		platform::ParameterBlock::PlatformParams::CreatePlatformParams(*this);
	}


	void ParameterBlock::RegisterAndCommit(std::shared_ptr<re::ParameterBlock> newPB, void const* data, size_t numBytes)
	{
		re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetParameterBlockAllocator();
		pbm.RegisterAndAllocateParameterBlock(newPB, numBytes);

		// TODO: This should happen via ParameterBlock::CommitInternal, so we verify the typeIDHash on all paths
		pbm.Commit(newPB->GetUniqueID(), data);

		// Now that we've allocated and committed some data, we can finally perform platform creation (which buffers)
		platform::ParameterBlock::Create(*newPB);
	}


	void ParameterBlock::CommitInternal(void const* data, uint64_t typeIDHash)
	{
		SEAssert("Invalid type detected. Can only set data of the original type",
			typeIDHash == m_typeIDHash);
		SEAssert("Cannot set data of an immutable param block", m_updateType != UpdateType::Immutable);

		re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetParameterBlockAllocator();
		pbm.Commit(GetUniqueID(), data);

		m_isDirty = true;
	}


	void ParameterBlock::GetDataAndSize(void*& out_data, size_t& out_numBytes)
	{
		re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetParameterBlockAllocator();
		pbm.Get(GetUniqueID(), out_data, out_numBytes);
	}


	void ParameterBlock::Destroy()
	{
		platform::ParameterBlock::Destroy(*this);

		re::ParameterBlockAllocator& pbm = RenderManager::Get()->GetParameterBlockAllocator();
		pbm.Deallocate(GetUniqueID());
	}
}