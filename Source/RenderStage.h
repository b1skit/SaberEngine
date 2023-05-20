// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Batch.h"
#include "Camera.h"
#include "Context_Platform.h"
#include "MeshPrimitive.h"
#include "NamedObject.h"
#include "ParameterBlock.h"
#include "PipelineState.h"
#include "Shader.h"
#include "Shader_Platform.h"
#include "TextureTarget.h"


namespace re
{
	class RenderStage final : public virtual en::NamedObject
	{
	public:
		enum class RenderStageType
		{
			ColorOnly,
			DepthOnly,
			ColorAndDepth,

			RenderStageType_Count
		};

		typedef std::vector<std::tuple<std::string, std::shared_ptr<re::Texture>, std::shared_ptr<re::Sampler>>> RenderStageTextureAndSamplerInput;


	public:
		explicit RenderStage(std::string const& name);
				
		RenderStage(RenderStage const&);

		~RenderStage() = default;
		RenderStage(RenderStage&&) = default;

		void EndOfFrame(); // Clears per-frame data. Called by the owning RenderPipeline

		bool WritesColor() const { return m_writesColor; }; // Are any of the params.m_colorWriteMode channels enabled?

		void SetStagePipelineState(gr::PipelineState const& params);
		inline gr::PipelineState const& GetStagePipelineState() const { return m_pipelineState; }
		inline gr::PipelineState& GetStagePipelineState() { return m_pipelineState; } // Note: Do not modify. Use SetStagePipelineState instead

		void SetStageShader(std::shared_ptr<re::Shader>);
		re::Shader* GetStageShader() const;

		std::shared_ptr<re::TextureTargetSet const> GetTextureTargetSet() const;
		void SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet);

		// Per-frame values must be re-set every frame
		void SetPerFrameTextureInput(
			std::string const& shaderName, std::shared_ptr<re::Texture> tex, std::shared_ptr<re::Sampler> sampler);
		inline RenderStageTextureAndSamplerInput const& GetPerFrameTextureInputs() const { return m_perFrameTextureSamplerInputs; }

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
		std::shared_ptr<re::Shader> m_stageShader;
		std::shared_ptr<re::TextureTargetSet> m_textureTargetSet;
		
		gr::PipelineState m_pipelineState;
		bool m_writesColor;

		// Per-frame members are cleared every frame
		RenderStageTextureAndSamplerInput m_perFrameTextureSamplerInputs;
		std::vector<std::shared_ptr<re::ParameterBlock>> m_perFrameParamBlocks;

		std::vector<std::shared_ptr<re::ParameterBlock >> m_permanentParamBlocks;

		std::vector<re::Batch> m_stageBatches;
		uint32_t m_batchFilterMask;

		
	private:
		RenderStage() = delete;
		RenderStage& operator=(RenderStage const&) = delete;
	};


	inline void RenderStage::SetStageShader(std::shared_ptr<re::Shader> shader)
	{
		m_stageShader = shader;
	}


	inline re::Shader* RenderStage::GetStageShader() const
	{
		return m_stageShader.get();
	}


	inline std::shared_ptr<re::TextureTargetSet const> RenderStage::GetTextureTargetSet() const
	{
		return m_textureTargetSet;
	}
}