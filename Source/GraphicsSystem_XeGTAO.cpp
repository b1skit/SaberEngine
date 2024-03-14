// © 2024 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_XeGTAO.h"
#include "GraphicsSystemManager.h"
#include "Sampler.h"
#include "Shader.h"


namespace
{
	struct SEXeGTAOSettings
	{
		float g_enabled; // Boolean: Output 100% white if disabled (g_enabled = 0), AO otherwise
		float _padding[3];
	};


	SEXeGTAOSettings CreateXeGTAOSettingsParamsData(gr::XeGTAOGraphicsSystem::Quality quality)
	{
		SEXeGTAOSettings settings;

		// TODO: Support more settings...
		switch (quality)
		{
		case gr::XeGTAOGraphicsSystem::Quality::Disabled:
		{
			settings.g_enabled = 0.f;
		}
		break;
		case gr::XeGTAOGraphicsSystem::Quality::Low:
		{
			settings.g_enabled = 1.f;
		}
		break;
		case gr::XeGTAOGraphicsSystem::Quality::Med:
		{
			settings.g_enabled = 1.f;
		}
		break;
		case gr::XeGTAOGraphicsSystem::Quality::High:
		{
			settings.g_enabled = 1.f;
		}
		break;
		case gr::XeGTAOGraphicsSystem::Quality::Ultra:
		{
			settings.g_enabled = 1.f;
		}
		break;
		default: SEAssertF("Invalid quality level");
		}

		return settings;
	}


	// Pack the settings struct we pass to the XeGTAO library to assemble our buffer data
	void ConfigureGTAOSettings(
		gr::XeGTAOGraphicsSystem::Quality quality,
		gr::XeGTAOGraphicsSystem::Denoise denoisePasses,
		XeGTAO::GTAOSettings& gtaoSettings)
	{
		// TODO: Support the auto-tuned settings path

		gtaoSettings.QualityLevel = static_cast<int>(quality);
		SEAssert(gtaoSettings.QualityLevel >= 0 && gtaoSettings.QualityLevel <= 4, "Unexpected quality value");

		gtaoSettings.DenoisePasses = static_cast<int>(denoisePasses); // 0/1/2/3 : Disabled/Sharp/Medium/Soft
		SEAssert(gtaoSettings.DenoisePasses >= 0 && gtaoSettings.DenoisePasses <= 3, "Unexpected denoise value");
	}


	XeGTAO::GTAOConstants GetGTAOConstantsData(
		int xRes, int yRes, XeGTAO::GTAOSettings const& settings, glm::mat4 const& projection)
	{
		XeGTAO::GTAOConstants gtaoConstants;
		XeGTAO::GTAOUpdateConstants(gtaoConstants,
			xRes,
			yRes,
			settings,
			&projection[0][0],
			false, // Row/colMajor: False (i.e. use column major), as GLM stores CPU-side matrices in col-major order
			0); // No TAA

		return gtaoConstants;
	}


	std::shared_ptr<re::Texture> CreateHilbertLUT()
	{
		constexpr uint32_t k_texWidthHeight = 64;
		constexpr uint32_t k_bytesPerTexel = sizeof(uint16_t);
		std::vector<std::vector<uint8_t>> texData;

		texData.resize(1, std::vector<uint8_t>(k_texWidthHeight * k_texWidthHeight * k_bytesPerTexel));
		
		uint16_t* dataPtr = reinterpret_cast<uint16_t*>(texData[0].data());

		for (uint32_t x = 0; x < 64; x++)
		{
			for (uint32_t y = 0; y < 64; y++)
			{
				uint32_t r2index = XeGTAO::HilbertIndex(x, y);
				SEAssert(r2index < 65536, "Invalid value generated");

				dataPtr[x + (k_texWidthHeight * y)] = static_cast<uint16_t>(r2index);
			}
		}

		re::Texture::TextureParams hilbertLUTParams{};
		hilbertLUTParams.m_width = k_texWidthHeight;
		hilbertLUTParams.m_height = k_texWidthHeight;
		hilbertLUTParams.m_faces = 1;

		hilbertLUTParams.m_usage = re::Texture::Usage::Color;
		hilbertLUTParams.m_dimension = re::Texture::Dimension::Texture2D;
		hilbertLUTParams.m_format = re::Texture::Format::R16_UNORM;
		hilbertLUTParams.m_colorSpace = re::Texture::ColorSpace::Linear;

		hilbertLUTParams.m_mipMode = re::Texture::MipMode::None;
		hilbertLUTParams.m_multisampleMode = re::Texture::MultisampleMode::Disabled;

		hilbertLUTParams.m_addToSceneData = false;

		return re::Texture::Create("HilbertLUT", hilbertLUTParams, std::move(texData));
	}
}


namespace gr
{
	constexpr char const* k_gsName = "XeGTAO Graphics System";


	XeGTAOGraphicsSystem::XeGTAOGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
		, m_xRes(0)
		, m_yRes(0)
		, m_XeGTAOQuality(Quality::Ultra)
		, m_XeGTAODenoiseMode(Denoise::Soft)
		, m_denoiseFinalOutputIdx(0) // Updated duringe Create()
		, m_isDirty(true) // Cleared in PreRender()
	{
	}


	void XeGTAOGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		GBufferGraphicsSystem* gbufferGS = m_graphicsSystemManager->GetGraphicsSystem<GBufferGraphicsSystem>();

		m_xRes = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		m_yRes = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);

		m_hilbertLUT = CreateHilbertLUT();

		// Our own settings buffer:
		m_SEXeGTAOSettings = re::Buffer::Create(
			"SEXeGTAOSettings", 
			CreateXeGTAOSettingsParamsData(m_XeGTAOQuality),
			re::Buffer::Mutable);
		
		// TODO: Output bent normals
		// NOTE: This will require recreating the pipeline if we change this value (as targets depend on it)
		const bool outputBentNormals = false;

		// XeGTAO::GTAOConstants buffer. Note: We pass an identity projection matrix for now; we'll populate
		// the real one during PreRender()
		ConfigureGTAOSettings(m_XeGTAOQuality, m_XeGTAODenoiseMode, m_settings);
		const XeGTAO::GTAOConstants gtaoConstants = GetGTAOConstantsData(m_xRes, m_yRes, m_settings, glm::mat4(1.f));

		constexpr char const* k_bufferShaderName = "SEGTAOConstants"; // "GTAOConstants" is already defined for us
		m_XeGTAOConstants = 
			re::Buffer::Create(k_bufferShaderName, gtaoConstants, re::Buffer::Type::Mutable);


		// Depth prefilter stage:
		m_prefilterDepthsShader = re::Shader::GetOrCreate("XeGTAO_PrefilterDepths", re::PipelineState());

		m_prefilterDepthsStage = 
			re::RenderStage::CreateComputeStage("XeGTAO: Prefilter depths stage", re::RenderStage::ComputeStageParams{});
		m_prefilterDepthsStage->SetStageShader(m_prefilterDepthsShader);

		// Depth prefilter target:
		m_prefilterDepthsTargets = re::TextureTargetSet::Create("XeGTAO: Prefilter depths targets");
		
		re::Texture::TextureParams prefilterDepthTargetParams{};
		prefilterDepthTargetParams.m_width = m_xRes;
		prefilterDepthTargetParams.m_height = m_yRes;
		prefilterDepthTargetParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ComputeTarget | re::Texture::Usage::Color);
		prefilterDepthTargetParams.m_dimension = re::Texture::Dimension::Texture2D;
		prefilterDepthTargetParams.m_format = re::Texture::Format::R16F;
		prefilterDepthTargetParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		prefilterDepthTargetParams.m_mipMode = re::Texture::MipMode::Allocate;
		prefilterDepthTargetParams.m_addToSceneData = false;

		std::shared_ptr<re::Texture> prefilteredDepthTargetTex = 
			re::Texture::Create("XeGTAO: Prefiltered depths", prefilterDepthTargetParams);

		re::TextureTarget::TargetParams prefilteredDepthTargetParams{};

		// Mip 0:
		prefilteredDepthTargetParams.m_targetMip = 0;
		m_prefilterDepthsTargets->SetColorTarget(0, prefilteredDepthTargetTex, prefilteredDepthTargetParams);

		// Mip 1:
		prefilteredDepthTargetParams.m_targetMip = 1;
		m_prefilterDepthsTargets->SetColorTarget(1, prefilteredDepthTargetTex, prefilteredDepthTargetParams);

		// Mip 2:
		prefilteredDepthTargetParams.m_targetMip = 2;
		m_prefilterDepthsTargets->SetColorTarget(2, prefilteredDepthTargetTex, prefilteredDepthTargetParams);

		// Mip 3:
		prefilteredDepthTargetParams.m_targetMip = 3;
		m_prefilterDepthsTargets->SetColorTarget(3, prefilteredDepthTargetTex, prefilteredDepthTargetParams);

		// Mip 4:
		prefilteredDepthTargetParams.m_targetMip = 4;
		m_prefilterDepthsTargets->SetColorTarget(4, prefilteredDepthTargetTex, prefilteredDepthTargetParams);

		m_prefilterDepthsStage->SetTextureTargetSet(m_prefilterDepthsTargets);

		// Attach the depth buffer as an input to the depth prefilter stage:
		re::TextureTarget const* gbufferDepthTarget = gbufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget();

		m_prefilterDepthsStage->AddTextureInput(
			"Depth0", 
			gbufferDepthTarget->GetTexture(), 
			re::Sampler::GetSampler("ClampMinMagMipPoint"), 
			0);

		// Append the depth prefilter stage:
		pipeline.AppendRenderStage(m_prefilterDepthsStage);

		
		// Main pass:
		CreateMainStageShader(m_XeGTAOQuality); // We lazily create these as they're needed

		m_mainStage = re::RenderStage::CreateComputeStage("XeGTAO: Main stage", re::RenderStage::ComputeStageParams{});
		m_mainStage->SetStageShader(m_mainShaders[m_XeGTAOQuality]);

		// Main stage targets:
		m_mainTargets = re::TextureTargetSet::Create("XeGTAO: Main targets");

		re::Texture::Format workingAOTermFormat = re::Texture::Format::R8_UINT;
		if (outputBentNormals)
		{
			workingAOTermFormat = re::Texture::Format::R32_UINT;
		}

		re::Texture::TextureParams workingAOTexParams{};
		workingAOTexParams.m_width = m_xRes;
		workingAOTexParams.m_height = m_yRes;
		workingAOTexParams.m_usage =
			static_cast<re::Texture::Usage>(re::Texture::Usage::ComputeTarget | re::Texture::Usage::Color);
		workingAOTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		workingAOTexParams.m_format = workingAOTermFormat;
		workingAOTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		workingAOTexParams.m_mipMode = re::Texture::MipMode::None;
		workingAOTexParams.m_addToSceneData = false;

		std::shared_ptr<re::Texture> workingAOTex = re::Texture::Create("XeGTAO: Working AO", workingAOTexParams);

		re::TextureTarget::TargetParams defaultTargetParams{};

		m_mainTargets->SetColorTarget(0, workingAOTex, defaultTargetParams);


		re::Texture::TextureParams workingEdgesTexParams{};
		workingEdgesTexParams.m_width = m_xRes;
		workingEdgesTexParams.m_height = m_yRes;
		workingEdgesTexParams.m_usage =
			static_cast<re::Texture::Usage>(re::Texture::Usage::ComputeTarget | re::Texture::Usage::Color);
		workingEdgesTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		workingEdgesTexParams.m_format = re::Texture::Format::R8_UNORM;
		workingEdgesTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		workingEdgesTexParams.m_mipMode = re::Texture::MipMode::None;
		workingEdgesTexParams.m_addToSceneData = false;

		std::shared_ptr<re::Texture> workingEdgesTex = 
			re::Texture::Create("XeGTAO: Working Edges", workingEdgesTexParams);

		constexpr uint8_t k_workingEdgesIdx = 1;
		m_mainTargets->SetColorTarget(k_workingEdgesIdx, workingEdgesTex, defaultTargetParams);

		m_mainStage->SetTextureTargetSet(m_mainTargets);

		// Main stage texture inputs:
		m_mainStage->AddTextureInput(
			"PrefilteredDepth",
			prefilteredDepthTargetTex,
			re::Sampler::GetSampler("ClampMinMagMipPoint"));

		re::TextureTarget const& gbufferNormalTarget = 
			gbufferGS->GetFinalTextureTargetSet()->GetColorTarget(GBufferGraphicsSystem::GBufferWNormal);

		m_mainStage->AddTextureInput(
			"GBufferWorldNormal",
			gbufferNormalTarget.GetTexture(),
			re::Sampler::GetSampler("ClampMinMagMipPoint"),
			0);

		m_mainStage->AddTextureInput(
			k_hilbertLutTexName,
			m_hilbertLUT,
			re::Sampler::GetSampler("ClampMinMagMipPoint"));

		// Append the main stage:
		pipeline.AppendRenderStage(m_mainStage);


		// Denoise passes:
		m_denoiseShader = re::Shader::GetOrCreate("XeGTAO_Denoise", re::PipelineState());
		m_lastPassDenoiseShader = re::Shader::GetOrCreate("XeGTAO_DenoiseLastPass", re::PipelineState());

		// Always need at least 1 pass to ensure the final target is filled, even if denoising or AO is disabled
		const uint8_t numDenoisePasses = std::max((uint8_t)1, static_cast<uint8_t>(m_XeGTAOQuality));
		m_denoiseStages.resize(numDenoisePasses, nullptr);

		const uint8_t lastPassIdx = (numDenoisePasses - 1);
		m_denoiseFinalOutputIdx = lastPassIdx % 2;

		// Denoise ping-poing target sets:
		re::Texture::TextureParams denoiseTargetParams{};

		for (uint8_t targetIdx = 0; targetIdx < 2; targetIdx++)
		{
			m_denoisePingPongTargets[targetIdx] = re::TextureTargetSet::Create(
				std::format("XeGTAO: Denoise targets {}/2", (targetIdx + 1)).c_str());
		}

		// Create our first ping-pong target:
		std::shared_ptr<re::Texture> denoiseTarget = re::Texture::Create("XeGTAO: Denoise target", workingAOTexParams);
		m_denoisePingPongTargets[0]->SetColorTarget(0, denoiseTarget, defaultTargetParams);

		// We reuse the working AO buffer as our 2nd target
		m_denoisePingPongTargets[1]->SetColorTarget(0, workingAOTex, defaultTargetParams);
		
		for (uint8_t passIdx = 0; passIdx < numDenoisePasses; passIdx++)
		{
			m_denoiseStages[passIdx] = re::RenderStage::CreateComputeStage(
				std::format("XeGTAO: Denoise stage {}/{}", (passIdx + 1), numDenoisePasses).c_str(),
					re::RenderStage::ComputeStageParams{});

			const bool isLastPass = passIdx == lastPassIdx;
			if (isLastPass)
			{
				m_denoiseStages[passIdx]->SetStageShader(m_lastPassDenoiseShader);
			}
			else
			{
				m_denoiseStages[passIdx]->SetStageShader(m_denoiseShader);
			}
			
			// Set the appropriate ping/pong target set, and add the working AO target as input 
			// Note: We reuse the working AO target after the 1st denoise iteration
			if (passIdx % 2 == 1) // Odd numbers: 1, 3, ...
			{
				// All passes: Sample the previous denoise output:
				m_denoiseStages[passIdx]->AddTextureInput(
					"SourceAO",
					denoiseTarget, // Read from the denoise target texture
					re::Sampler::GetSampler("ClampMinMagMipPoint"));

				m_denoiseStages[passIdx]->SetTextureTargetSet(m_denoisePingPongTargets[1]);
			}
			else // Even numbers: 0, 2, ...
			{
				// First pass: Sample the working AO
				// Subsequent passes: sample the interim denoising results from the same buffer
				m_denoiseStages[passIdx]->AddTextureInput(
					"SourceAO",
					workingAOTex, // Read from the working AO texture
					re::Sampler::GetSampler("ClampMinMagMipPoint"));

				m_denoiseStages[passIdx]->SetTextureTargetSet(m_denoisePingPongTargets[0]);
			}

			// Add the working edges texture as an input:
			m_denoiseStages[passIdx]->AddTextureInput(
				"SourceEdges",
				m_mainTargets->GetColorTarget(k_workingEdgesIdx).GetTexture(),
				re::Sampler::GetSampler("ClampMinMagMipPoint"));

			pipeline.AppendRenderStage(m_denoiseStages[passIdx]);
		}
	}


	void XeGTAOGraphicsSystem::PreRender()
	{	
		if (m_isDirty)
		{
			gr::Camera::RenderData const& mainCamRenderData = m_graphicsSystemManager->GetActiveCameraRenderData();

			m_XeGTAOConstants->Commit(
				GetGTAOConstantsData(m_xRes, m_yRes, m_settings, mainCamRenderData.m_cameraParams.g_projection));

			m_SEXeGTAOSettings->Commit(CreateXeGTAOSettingsParamsData(m_XeGTAOQuality));

			m_isDirty = false;
		}

		CreateBatches();
	}


	void XeGTAOGraphicsSystem::CreateBatches()
	{
		SEAssert(m_xRes == en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey) && 
			m_yRes == en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey),
			"Resolution has changed, this graphics system needs to be recreated");

		// Depth pre-filter stage:
		if (m_prefilterDepthComputeBatch == nullptr)
		{
			re::Batch::ComputeParams prefilterDepthBatchParams{};

			// The depth prefilter shader executes numthreads(8, 8, 1), with each logical thread handling a 2x2 block.
			// Thus, we perform a total of (width, height) / (16, 16) dispatches, but round up via an integer floor) to
			// handle the edges
			constexpr int k_blockSize = 16;
			constexpr int k_extra = k_blockSize - 1;
			
			prefilterDepthBatchParams.m_threadGroupCount = glm::uvec3(
				(m_xRes + k_extra) / k_blockSize,
				(m_yRes + k_extra) / k_blockSize,
				1);

			m_prefilterDepthComputeBatch = 
				std::make_unique<re::Batch>(re::Batch::Lifetime::Permanent, prefilterDepthBatchParams);

			m_prefilterDepthComputeBatch->SetBuffer(m_XeGTAOConstants);
		}
		m_prefilterDepthsStage->AddBatch(*m_prefilterDepthComputeBatch);

		// Main stage:
		if (m_mainBatch == nullptr)
		{
			re::Batch::ComputeParams mainBatchParams{};

			// The main stage executes numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1), as per the values
			// defined in XeeGTAO.h (and mirrored in our XeGTAOCommon.hlsli library).
			constexpr int k_extraX = XE_GTAO_NUMTHREADS_X - 1;
			constexpr int k_extraY = XE_GTAO_NUMTHREADS_Y - 1;

			mainBatchParams.m_threadGroupCount = glm::uvec3(
				(m_xRes + k_extraX) / XE_GTAO_NUMTHREADS_X,
				(m_yRes + k_extraY) / XE_GTAO_NUMTHREADS_Y,
				1);

			m_mainBatch = std::make_unique<re::Batch>(re::Batch::Lifetime::Permanent, mainBatchParams);

			m_mainBatch->SetBuffer(m_XeGTAOConstants);
			m_mainBatch->SetBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		}
		m_mainStage->AddBatch(*m_mainBatch);

		// Denoise stages:
		if (m_denoiseBatch == nullptr || m_lastPassDenoiseBatch == nullptr)
		{
			re::Batch::ComputeParams denoiseBatchParams{};

			constexpr int k_extraX = (XE_GTAO_NUMTHREADS_X * 2) - 1;
			constexpr int k_extraY = XE_GTAO_NUMTHREADS_Y - 1;

			denoiseBatchParams.m_threadGroupCount = glm::uvec3(
				(m_xRes + k_extraX) / (XE_GTAO_NUMTHREADS_X * 2),
				(m_yRes + k_extraY) / (XE_GTAO_NUMTHREADS_Y),
				1);

			m_denoiseBatch = std::make_unique<re::Batch>(re::Batch::Lifetime::Permanent, denoiseBatchParams);
			m_lastPassDenoiseBatch = std::make_unique<re::Batch>(re::Batch::Lifetime::Permanent, denoiseBatchParams);

			m_denoiseBatch->SetBuffer(m_XeGTAOConstants);
			m_lastPassDenoiseBatch->SetBuffer(m_XeGTAOConstants);

			m_lastPassDenoiseBatch->SetBuffer(m_SEXeGTAOSettings); // Needed for final stage ONLY
		}

		const uint8_t lastStageIdx = util::CheckedCast<uint8_t>(m_denoiseStages.size() - 1);
		for (uint8_t denoiseIdx = 0; denoiseIdx < m_denoiseStages.size(); denoiseIdx++)
		{
			if (denoiseIdx == lastStageIdx)
			{
				m_denoiseStages[denoiseIdx]->AddBatch(*m_lastPassDenoiseBatch);
			}
			else
			{
				m_denoiseStages[denoiseIdx]->AddBatch(*m_denoiseBatch);
			}
		}
	}


	void XeGTAOGraphicsSystem::CreateMainStageShader(Quality quality)
	{
		switch (m_XeGTAOQuality)
		{
		case Quality::Disabled:
		case Quality::Low:
		{
			m_mainShaders[Quality::Low] = re::Shader::GetOrCreate("XeGTAO_MainPass_Low", re::PipelineState());
		}
		break;
		case Quality::Med:
		{
			m_mainShaders[Quality::Med] = re::Shader::GetOrCreate("XeGTAO_MainPass_Med", re::PipelineState());
		}
		break;
		case Quality::High:
		{
			m_mainShaders[Quality::High] = re::Shader::GetOrCreate("XeGTAO_MainPass_High", re::PipelineState());
		}
		break;
		case Quality::Ultra:
		{
			m_mainShaders[Quality::Ultra] = re::Shader::GetOrCreate("XeGTAO_MainPass_Ultra", re::PipelineState());
		}
		break;
		default: SEAssertF("Invalid quality");
		}
	}

	void XeGTAOGraphicsSystem::SetQuality(Quality quality)
	{
		if (quality == m_XeGTAOQuality)
		{
			return;
		}
		m_XeGTAOQuality = quality;

		// Ensure the shader has been created:
		if (m_mainShaders[m_XeGTAOQuality] == nullptr)
		{
			CreateMainStageShader(quality);
		}

		// We still need a shader, even if the quality mode is disabled
		constexpr Quality k_minQuality = Quality::Low;
		m_mainStage->SetStageShader(m_mainShaders[m_XeGTAOQuality == Quality::Disabled ? k_minQuality : m_XeGTAOQuality]);

		// Something has changed: Mark ourselves as dirty!
		m_isDirty = true;
	}


	void XeGTAOGraphicsSystem::ShowImGuiWindow()
	{
		char const* qualitySettings[] = {"Disabled", "Low", "Med", "High", "Ultra"};
		int currentQuality = static_cast<int>(m_XeGTAOQuality);
		m_isDirty |= ImGui::Combo("Quality", &currentQuality, qualitySettings, IM_ARRAYSIZE(qualitySettings));
		SetQuality(static_cast<Quality>(currentQuality));

		m_isDirty |= ImGui::SliderFloat("Effect radius", &m_settings.Radius, 0.f, 5.f);

		if (ImGui::CollapsingHeader("Heuristic settings"))
		{
			ImGui::Indent();

			m_isDirty |= ImGui::SliderFloat("Radius multiplier", &m_settings.RadiusMultiplier, 0.f, 5.f);
			m_isDirty |= ImGui::SliderFloat("Falloff range", &m_settings.FalloffRange, 0.f, 5.f);
			m_isDirty |= ImGui::SliderFloat("Sample distribution power", &m_settings.SampleDistributionPower, 0.f, 5.f);
			m_isDirty |= ImGui::SliderFloat("Thin occluder compensation", &m_settings.ThinOccluderCompensation, 0.f, 5.f);
			m_isDirty |= ImGui::SliderFloat("Final power value", &m_settings.FinalValuePower, 0.f, 5.f);
			m_isDirty |= ImGui::SliderFloat(
				"Depth MIP sampling offset", 
				&m_settings.DepthMIPSamplingOffset, 
				0.f, 
				m_prefilterDepthsTargets->GetNumColorTargets());

			if (ImGui::Button("Reset to defaults"))
			{
				m_isDirty = true;
				m_settings = XeGTAO::GTAOSettings{};
			}

			ImGui::Unindent();
		}
	}
}