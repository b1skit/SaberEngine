#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Shader.h"
#include "Shader_Platform.h"
#include "Camera.h"
#include "TextureTarget.h"
#include "Context_Platform.h"
#include "MeshPrimitive.h"
#include "NamedObject.h"
#include "ParameterBlock.h"
#include "Batch.h"


namespace gr
{
	class RenderStage : public virtual en::NamedObject
	{
	public:
		struct StageShaderUniform
		{
			std::string m_uniformName;
			void const* m_value;
			platform::Shader::UniformType const m_type;
			int m_count;
		};

		enum class RenderStageType
		{
			ColorOnly,
			DepthOnly,
			ColorAndDepth,

			RenderStageType_Count
		};


		struct PipelineStateParams // Platform/context configuration:
		{			
			platform::Context::ClearTarget		m_targetClearMode	= platform::Context::ClearTarget::None;
			platform::Context::FaceCullingMode	m_faceCullingMode	= platform::Context::FaceCullingMode::Back;
			platform::Context::BlendMode		m_srcBlendMode		= platform::Context::BlendMode::One;
			platform::Context::BlendMode		m_dstBlendMode		= platform::Context::BlendMode::One;
			platform::Context::DepthTestMode	m_depthTestMode		= platform::Context::DepthTestMode::GEqual;
			platform::Context::DepthWriteMode	m_depthWriteMode	= platform::Context::DepthWriteMode::Enabled;
			platform::Context::ColorWriteMode	m_colorWriteMode =
			{ 
				platform::Context::ColorWriteMode::ChannelMode::Enabled, // R
				platform::Context::ColorWriteMode::ChannelMode::Enabled, // G
				platform::Context::ColorWriteMode::ChannelMode::Enabled, // B
				platform::Context::ColorWriteMode::ChannelMode::Enabled  // A
			};

			struct
			{
				uint32_t m_targetFace = 0;
				uint32_t m_targetMip = 0;
			} m_textureTargetSetConfig;
		};


	public:
		explicit RenderStage(std::string const& name);
				
		RenderStage(RenderStage const&);

		~RenderStage() = default;
		RenderStage(RenderStage&&) = default;

		void InitializeForNewFrame(); // Clears per-frame data

		bool WritesColor() const { return m_writesColor; }; // Are any of the params.m_colorWriteMode channels enabled?

		void SetStagePipelineStateParams(PipelineStateParams const& params);
		inline PipelineStateParams const& GetStagePipelineStateParams() const { return m_stageParams; }

		inline std::shared_ptr<gr::Shader>& GetStageShader() { return m_stageShader; }
		inline std::shared_ptr<gr::Shader const> GetStageShader() const { return m_stageShader; }

		inline gr::Camera*& GetStageCamera() { return m_stageCam; }
		inline gr::Camera const* GetStageCamera() const { return m_stageCam; }

		inline re::TextureTargetSet& GetTextureTargetSet() { return m_textureTargetSet; }
		inline re::TextureTargetSet const& GetTextureTargetSet() const { return m_textureTargetSet; }

		// Helper: Simultaneously binds a texture and sampler by name to the stage shader
		void SetTextureInput(
			std::string const& shaderName, std::shared_ptr<gr::Texture const> tex, std::shared_ptr<gr::Sampler const> sampler);

		// Per-frame uniforms are set every frame
		inline std::vector<StageShaderUniform> const& GetPerFrameShaderUniforms() const { return m_perFrameShaderUniforms; }

		template <typename T>
		void SetPerFrameShaderUniform(
			std::string const& uniformName, T const& value, platform::Shader::UniformType const& type, int const count);

		void AddPermanentParameterBlock(std::shared_ptr<re::ParameterBlock> pb);
		inline std::vector<std::shared_ptr<re::ParameterBlock>> const& GetPermanentParameterBlocks() const { return m_permanentParamBlocks; }
		
		void AddSingleFrameParameterBlock(std::shared_ptr<re::ParameterBlock> pb);
		inline std::vector<std::shared_ptr<re::ParameterBlock>> const& GetPerFrameParameterBlocks() const { return m_perFrameParamBlocks; }

		// Stage Batches:
		inline std::vector<re::Batch> const& GetStageBatches() const { return m_stageBatches; }
		void AddBatches(std::vector<re::Batch> const& batches);
		void AddBatch(re::Batch const& batch);

		inline uint32_t GetBatchFilterMask() const { return m_batchFilterMask; }
		void SetBatchFilterMaskBit(re::Batch::Filter filterBit);

	private:
		std::shared_ptr<gr::Shader> m_stageShader;
		re::TextureTargetSet m_textureTargetSet;
		gr::Camera* m_stageCam;
		
		PipelineStateParams m_stageParams;
		bool m_writesColor;

		// Per-frame members are cleared every frame
		std::vector<StageShaderUniform> m_perFrameShaderUniforms; // TODO: Handle selection of face, miplevel when binding color/depth targets?
		std::vector<std::shared_ptr<const void>> m_perFrameShaderUniformValues; // Generic, per-frame data storage buffer
		
		std::vector<std::shared_ptr<re::ParameterBlock>> m_perFrameParamBlocks;
		std::vector<std::shared_ptr<re::ParameterBlock >> m_permanentParamBlocks;


		std::vector<re::Batch> m_stageBatches;
		uint32_t m_batchFilterMask;

		
	private:
		RenderStage() = delete;
		RenderStage& operator=(RenderStage const&) = delete;
	};

}