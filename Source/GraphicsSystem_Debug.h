// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class DebugGraphicsSystem final : public virtual GraphicsSystem
	{
	public:

		DebugGraphicsSystem();

		void Create(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		//void ShowImGuiWindow() override;


	private:
		void CreateBatches() override;


	private:
		std::shared_ptr<re::RenderStage> m_debugStage;
	};


	inline std::shared_ptr<re::TextureTargetSet const> DebugGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_debugStage->GetTextureTargetSet();
	}
}

