// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Camera.h"
#include "GraphicsSystem.h"
#include "ShadowMap.h"


namespace gr
{
	class ShadowsGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<ShadowsGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Shadows"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(ShadowsGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(ShadowsGraphicsSystem, PreRender))
			);
		}

		static constexpr util::HashKey k_cullingDataInput = "ViewCullingResults";
		static constexpr util::HashKey k_pointLightCullingDataInput = "PointLightCullingResults";
		static constexpr util::HashKey k_spotLightCullingDataInput = "SpotLightCullingResults";
		void RegisterInputs() override;

		static constexpr util::HashKey k_shadowTexturesOutput = "ShadowTextures";
		void RegisterOutputs() override;


	public:
		ShadowsGraphicsSystem(gr::GraphicsSystemManager*);
		~ShadowsGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&);
		void PreRender(DataDependencies const&);

		std::unordered_map<gr::RenderDataID, re::Texture const*> const& GetShadowTextures() const;


	private:
		void CreateBatches(DataDependencies const&);


	private:
		struct ShadowStageData
		{
			std::shared_ptr<re::RenderStage> m_renderStage;
			std::shared_ptr<re::TextureTargetSet> m_shadowTargetSet;
			std::shared_ptr<re::Buffer> m_shadowCamParamBlock;
		};

		std::unordered_map<gr::RenderDataID, ShadowStageData> m_directionalShadowStageData;
		std::unordered_map<gr::RenderDataID, ShadowStageData> m_pointShadowStageData;
		std::unordered_map<gr::RenderDataID, ShadowStageData> m_spotShadowStageData;

		// We maintain a parallel map of ALL light IDs -> textures, to simplify interactions with other graphics systems
		std::unordered_map<gr::RenderDataID, re::Texture const*> m_shadowTextures;


		void CreateRegister2DShadowStage(
			std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
			gr::RenderDataID,
			gr::ShadowMap::RenderData const&,
			gr::Camera::RenderData const&);


		void CreateRegisterCubeShadowStage(
			std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
			gr::RenderDataID,
			gr::ShadowMap::RenderData const&,
			gr::Transform::RenderData const&,
			gr::Camera::RenderData const&);
		

	private:
		// Pipeline:
		re::StagePipeline* m_stagePipeline;
		re::StagePipeline::StagePipelineItr m_directionalParentStageItr;
		re::StagePipeline::StagePipelineItr m_pointParentStageItr;
		re::StagePipeline::StagePipelineItr m_spotParentStageItr;
	};


	inline std::unordered_map<gr::RenderDataID, re::Texture const*> const& ShadowsGraphicsSystem::GetShadowTextures() const
	{
		return m_shadowTextures;
	}
}