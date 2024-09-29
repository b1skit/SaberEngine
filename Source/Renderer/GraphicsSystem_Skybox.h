// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class SkyboxGraphicsSystem final
		: public virtual GraphicsSystem 
		, public virtual IScriptableGraphicsSystem<SkyboxGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Skybox"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(SkyboxGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(SkyboxGraphicsSystem, PreRender))
			);
		}


		static constexpr util::HashKey k_skyboxTargetInput = "SkyboxTarget";
		static constexpr util::HashKey k_sceneDepthTexInput = "SceneDepth";
		void RegisterInputs() override;

		void RegisterOutputs() override;


	public:
		SkyboxGraphicsSystem(gr::GraphicsSystemManager*);
		~SkyboxGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();

		void ShowImGuiWindow() override;


	private:
		static constexpr char const* k_skyboxTexShaderName = "Tex0";

		std::shared_ptr<re::RenderStage> m_skyboxStage;
		re::Texture const* m_skyTexture;
		std::shared_ptr<re::Buffer> m_skyboxParams;

		// Fallback if no ambient light/IBL texture is found, but the flat color debug mode is not enabled
		std::shared_ptr<re::Texture> m_fallbackColorTex; 
		
		glm::vec3 m_backgroundColor;
		bool m_showBackgroundColor;
		bool m_isDirty;
	};
}