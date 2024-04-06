// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"

#include "XeGTAO.h"


namespace gr
{
	class XeGTAOGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<XeGTAOGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "XeGTAO"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(XeGTAOGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(XeGTAOGraphicsSystem, PreRender))
			);
		}


	public:
		XeGTAOGraphicsSystem(gr::GraphicsSystemManager*);

		~XeGTAOGraphicsSystem() override {}

		void InitPipeline(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		void ShowImGuiWindow() override;


	private:
		void CreateBatches() override;


	public:
		enum Quality : uint8_t
		{
			Disabled	= 0,
			Low			= 1,
			Med			= 2,
			High		= 3,
			Ultra		= 4,

			Quality_Count
		};
		enum class Denoise
		{
			Disabled	= 0,
			Sharp		= 1,
			Medium		= 2,
			Soft		= 3
		};

		void CreateMainStageShader(Quality);
		void SetQuality(Quality);


	private:
		std::shared_ptr<re::RenderStage> m_prefilterDepthsStage;
		std::shared_ptr<re::Shader> m_prefilterDepthsShader;
		std::shared_ptr<re::TextureTargetSet> m_prefilterDepthsTargets;
		std::unique_ptr<re::Batch> m_prefilterDepthComputeBatch;
			
		std::shared_ptr<re::RenderStage> m_mainStage;
		std::array<std::shared_ptr<re::Shader>, Quality::Quality_Count> m_mainShaders;
		std::shared_ptr<re::TextureTargetSet> m_mainTargets;
		std::unique_ptr<re::Batch> m_mainBatch;

		uint8_t m_denoiseFinalOutputIdx;
		std::vector<std::shared_ptr<re::RenderStage>> m_denoiseStages;
		std::shared_ptr<re::Shader> m_denoiseShader;
		std::shared_ptr<re::Shader> m_lastPassDenoiseShader;
		std::array<std::shared_ptr<re::TextureTargetSet>, 2> m_denoisePingPongTargets;
		std::unique_ptr<re::Batch> m_denoiseBatch;
		std::unique_ptr<re::Batch> m_lastPassDenoiseBatch;

		static constexpr char const* k_hilbertLutTexName = "g_srcHilbertLUT"; // As defined in 
		std::shared_ptr<re::Texture> m_hilbertLUT;
		
		XeGTAO::GTAOSettings m_settings; // Passed to the XeGTAO library to get the struct we pack into m_XeGTAOConstants
		std::shared_ptr<re::Buffer> m_XeGTAOConstants; // Our mirror of the XeGTAO constants block

		std::shared_ptr<re::Buffer> m_SEXeGTAOSettings; // Our own SaberEngine settings block
		
		Quality m_XeGTAOQuality;
		Denoise m_XeGTAODenoiseMode;
		bool m_isDirty;

		int m_xRes;
		int m_yRes;
	};



	inline std::shared_ptr<re::TextureTargetSet const> XeGTAOGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_denoiseStages[m_denoiseFinalOutputIdx]->GetTextureTargetSet();
	}
}