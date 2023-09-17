// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class BloomGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		explicit BloomGraphicsSystem(std::string name);

		BloomGraphicsSystem() = delete;
		~BloomGraphicsSystem() override {}

		void Create(re::RenderSystem&, re::StagePipeline& pipeline);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		void ShowImGuiWindow() override;


	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;

		std::shared_ptr<re::RenderStage> m_emissiveBlitStage;

		static constexpr uint32_t m_numDownSamplePasses = 2; // Scaling factor: # times we half the frame size
		static constexpr uint32_t m_numBlurPasses = 3; // How many pairs of horizontal + vertical blur passes to perform

		std::array<std::shared_ptr<re::RenderStage>, m_numDownSamplePasses> m_downResStages;
		std::array<std::shared_ptr<re::RenderStage>, m_numBlurPasses * 2>	m_blurStages;
		std::array<std::shared_ptr<re::RenderStage>, m_numDownSamplePasses> m_upResStages;

		struct LuminanceThresholdParams
		{
			glm::vec4 g_sigmoidParams = glm::vec4(30.f, 1.2f, 0.f, 0.f); // .x = Sigmoid ramp power, .y = Sigmoid speed, .zw = unused

			static constexpr char const* const s_shaderName = "LuminanceThresholdParams";
		} m_luminanceThresholdParams;
		std::shared_ptr<re::ParameterBlock> m_luminanceThresholdParamBlock;

		std::shared_ptr<re::ParameterBlock> m_horizontalBloomParams;
		std::shared_ptr<re::ParameterBlock> m_verticalBloomParams;
	};


	inline std::shared_ptr<re::TextureTargetSet const> BloomGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_emissiveBlitStage->GetTextureTargetSet();
	}
}