// © 2023 Adam Badke. All rights reserved.

#include "GraphicsSystem_TempDebug.h"


namespace gr
{
	TempDebugGraphicsSystem::TempDebugGraphicsSystem(std::string name)
		: GraphicsSystem(name)
		, NamedObject(name)
		, m_tempDebugStage("DX12 temp debug stage")
	{
	}


	void TempDebugGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		
	}


	void TempDebugGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();
	}


	std::shared_ptr<re::TextureTargetSet> TempDebugGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_tempDebugStage.GetTextureTargetSet();
	}


	void TempDebugGraphicsSystem::CreateBatches()
	{
		
	}
}