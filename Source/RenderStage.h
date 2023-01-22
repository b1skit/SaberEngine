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
		struct StageShaderUniform
		{
			std::string m_uniformName;
			void* m_value;
			re::Shader::UniformType const m_type;
			int m_count;
		};

		enum class RenderStageType
		{
			ColorOnly,
			DepthOnly,
			ColorAndDepth,

			RenderStageType_Count
		};


	public:
		explicit RenderStage(std::string const& name);
				
		RenderStage(RenderStage const&);

		~RenderStage() = default;
		RenderStage(RenderStage&&) = default;

		void EndOfFrame(); // Clears per-frame data. Called by the owning RenderPipeline

		bool WritesColor() const { return m_writesColor; }; // Are any of the params.m_colorWriteMode channels enabled?

		void SetStagePipelineState(gr::PipelineState const& params);
		inline gr::PipelineState const& GetStagePipelineState() const { return m_pipelineState; }

		inline std::shared_ptr<re::Shader>& GetStageShader() { return m_stageShader; }
		inline std::shared_ptr<re::Shader> GetStageShader() const { return m_stageShader; }

		inline void SetStageCamera(gr::Camera* stageCam) { m_stageCam = stageCam; }
		inline gr::Camera* GetStageCamera() const { return m_stageCam; }

		inline std::shared_ptr<re::TextureTargetSet> GetTextureTargetSet() const { return m_textureTargetSet; }
		void SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet);

		// Helper: Simultaneously binds a texture and sampler by name to the stage shader
		void SetTextureInput(
			std::string const& shaderName, std::shared_ptr<re::Texture> tex, std::shared_ptr<re::Sampler> sampler);

		// Per-frame uniforms are set every frame
		inline std::vector<StageShaderUniform> const& GetPerFrameShaderUniforms() const { return m_perFrameShaderUniforms; }

		template <typename T>
		void SetPerFrameShaderUniform(
			std::string const& uniformName, T const& value, re::Shader::UniformType const& type, int const count);

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
		gr::Camera* m_stageCam;
		
		gr::PipelineState m_pipelineState;
		bool m_writesColor;

		// Per-frame members are cleared every frame
		std::vector<StageShaderUniform> m_perFrameShaderUniforms; // TODO: Handle selection of face, miplevel when binding color/depth targets?
		std::vector<std::shared_ptr<void>> m_perFrameShaderUniformValues; // Generic, per-frame data storage buffer
		
		std::vector<std::shared_ptr<re::ParameterBlock>> m_perFrameParamBlocks;
		std::vector<std::shared_ptr<re::ParameterBlock >> m_permanentParamBlocks;


		std::vector<re::Batch> m_stageBatches;
		uint32_t m_batchFilterMask;

		
	private:
		RenderStage() = delete;
		RenderStage& operator=(RenderStage const&) = delete;
	};


	template <typename T>
	void RenderStage::SetPerFrameShaderUniform(
		std::string const& uniformName, T const& value, re::Shader::UniformType const& type, int const count)
	{
		// Dynamically allocate a copy of value so we have a pointer to it when we need for the current frame
		m_perFrameShaderUniformValues.emplace_back(std::make_shared<T>(value));

		void* valuePtr;
		if (count > 1)
		{
			// Assume if count > 1, we've recieved multiple values packed into a std::vector. 
			// Thus, we must store the address of the first element of the vector (NOT the address of the vector object!)
			valuePtr = &(reinterpret_cast<std::vector<T>*>(m_perFrameShaderUniformValues.back().get())->at(0));
		}
		else
		{
			valuePtr = m_perFrameShaderUniformValues.back().get();
		}

		m_perFrameShaderUniforms.emplace_back(uniformName, valuePtr, type, count);
	}
}