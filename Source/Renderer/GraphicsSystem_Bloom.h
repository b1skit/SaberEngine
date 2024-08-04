// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class BloomGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<BloomGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Bloom"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(BloomGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(BloomGraphicsSystem, PreRender))
			);
		}

		static constexpr util::HashKey k_emissiveInput = "EmissiveLight";
		static constexpr util::HashKey k_bloomTargetInput = "BloomTarget";
		void RegisterInputs() override;

		static constexpr util::HashKey k_bloomResultOutput = "BloomResult";
		void RegisterOutputs() override;


	public:
		BloomGraphicsSystem(gr::GraphicsSystemManager*);

		~BloomGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline& pipeline, TextureDependencies const&, BufferDependencies const&);

		void PreRender(DataDependencies const&);
	

	private:
		void CreateBatches();


	private:
		std::shared_ptr<re::RenderStage> m_emissiveBlitStage;

		// Bloom compute:
		std::vector<std::shared_ptr<re::RenderStage>> m_bloomDownStages;
		std::vector<std::shared_ptr<re::Buffer>> m_bloomDownBuffers;

		std::vector<std::shared_ptr<re::RenderStage>> m_bloomUpStages;
		std::vector<std::shared_ptr<re::Buffer>> m_bloomUpBuffers;

		std::shared_ptr<re::Texture> m_bloomTargetTex;

		uint32_t m_firstUpsampleSrcMipLevel; // == # of upsample stages
	};
}