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

		static constexpr util::CHashKey k_wNormalInput = "SceneWNormal";
		static constexpr util::CHashKey k_depthInput = "SceneDepth";
		void RegisterInputs() override;

		static constexpr util::CHashKey k_aoOutput = "SSAOTex";
		void RegisterOutputs() override;


	public:
		XeGTAOGraphicsSystem(gr::GraphicsSystemManager*);

		~XeGTAOGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();

		void ShowImGuiWindow() override;


	private:
		void CreateBatches();


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

		void SetQuality(Quality);


	private:
		std::shared_ptr<re::Stage> m_prefilterDepthsStage;
		core::InvPtr<re::Texture> m_prefilterDepthsTex;
		std::unique_ptr<re::Batch> m_prefilterDepthComputeBatch;
			
		std::shared_ptr<re::Stage> m_mainStage;
		core::InvPtr<re::Texture> m_workingAOTex;
		core::InvPtr<re::Texture> m_workingEdgesTargetTex;
		std::unique_ptr<re::Batch> m_mainBatch;

		uint8_t m_denoiseFinalOutputIdx;
		std::vector<std::shared_ptr<re::Stage>> m_denoiseStages;
		core::InvPtr<re::Texture> m_denoisePingTargetTex;
		std::unique_ptr<re::Batch> m_denoiseBatch;
		std::unique_ptr<re::Batch> m_lastPassDenoiseBatch;

		static constexpr char const* k_hilbertLutTexName = "g_srcHilbertLUT"; // As defined in 
		core::InvPtr<re::Texture> m_hilbertLUT;
		
		XeGTAO::GTAOSettings m_settings; // Passed to the XeGTAO library to get the struct we pack into m_XeGTAOConstants
		re::BufferInput m_XeGTAOConstants; // Our mirror of the XeGTAO constants block

		re::BufferInput m_SEXeGTAOSettings; // Our own SaberEngine settings block
		
		Quality m_XeGTAOQuality;
		Denoise m_XeGTAODenoiseMode;
		bool m_isDirty;

		uint32_t m_xRes;
		uint32_t m_yRes;
	};
}