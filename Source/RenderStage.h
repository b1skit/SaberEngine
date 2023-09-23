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
#include "Texture.h"
#include "TextureTarget.h"


namespace re
{
	class ComputeStage;

	class RenderStage : public virtual en::NamedObject
	{
	public:
		enum class RenderStageLifetime
		{
			SingleFrame,
			Permanent
		};
		enum class RenderStageType
		{
			Graphics,
			Compute,

			// TODO: Add specialist types: Fullscreen, etc
		};
		struct IStageParams
		{
			virtual ~IStageParams() = 0;
		};
		struct GraphicsStageParams final : public virtual IStageParams
		{
			// TODO: Populate this
			// Assert values are set when they're received to catch any GS's that need to be updated
		};
		struct ComputeStageParams final : public virtual IStageParams
		{
			// TODO: Populate this
		};

		struct RenderStageTextureAndSamplerInput
		{
			std::string m_shaderName;
			std::shared_ptr<Texture> m_texture;
			std::shared_ptr<Sampler> m_sampler;

			uint32_t m_srcMip = re::Texture::k_allMips;
		};


	public:
		static std::shared_ptr<RenderStage> CreateGraphicsStage(std::string const& name, GraphicsStageParams const&);
		static std::shared_ptr<RenderStage> CreateComputeStage(std::string const& name, ComputeStageParams const&);

		static std::shared_ptr<RenderStage> CreateSingleFrameGraphicsStage(std::string const& name, GraphicsStageParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameComputeStage(std::string const& name, ComputeStageParams const&);


		~RenderStage() = default;

		void EndOfFrame(); // Clears per-frame data. Called by the owning RenderPipeline

		RenderStageType GetStageType() const;
		RenderStageLifetime GetStageLifetime() const;
		IStageParams const* GetStageParams() const;

		void SetStagePipelineState(gr::PipelineState const& params);
		inline gr::PipelineState const& GetStagePipelineState() const  { return m_pipelineState; }

		void SetStageShader(std::shared_ptr<re::Shader>);
		re::Shader* GetStageShader() const;

		std::shared_ptr<re::TextureTargetSet const> GetTextureTargetSet() const;
		void SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet);

		// Per-frame values must be re-set every frame
		void AddTextureInput(
			std::string const& shaderName, 
			std::shared_ptr<re::Texture>, 
			std::shared_ptr<re::Sampler>, 
			uint32_t mipLevel = re::Texture::k_allMips);
		std::vector<RenderStage::RenderStageTextureAndSamplerInput> const& GetTextureInputs() const;

		void AddPermanentParameterBlock(std::shared_ptr<re::ParameterBlock> pb);
		inline std::vector<std::shared_ptr<re::ParameterBlock>> const& GetPermanentParameterBlocks() const { return m_permanentParamBlocks; }
		
		void AddSingleFrameParameterBlock(std::shared_ptr<re::ParameterBlock> pb);
		inline std::vector<std::shared_ptr<re::ParameterBlock>> const& GetPerFrameParameterBlocks() const { return m_singleFrameParamBlocks; }

		// Stage Batches:
		inline std::vector<re::Batch> const& GetStageBatches() const { return m_stageBatches; }
		void AddBatches(std::vector<re::Batch> const& batches);
		void AddBatch(re::Batch const& batch);

		inline uint32_t GetBatchFilterMask() const { return m_batchFilterMask; }
		void SetBatchFilterMaskBit(re::Batch::Filter filterBit);


	protected:
		explicit RenderStage(std::string const& name, std::unique_ptr<IStageParams>&&, RenderStageType, RenderStageLifetime);


	private:
		const RenderStageType m_type;
		const RenderStageLifetime m_lifetime;
		std::unique_ptr<IStageParams> m_stageParams;

		std::shared_ptr<re::Shader> m_stageShader;
		std::shared_ptr<re::TextureTargetSet> m_textureTargetSet;
		
		gr::PipelineState m_pipelineState;

		std::vector<RenderStageTextureAndSamplerInput> m_textureSamplerInputs;

		std::vector<std::shared_ptr<re::ParameterBlock>> m_singleFrameParamBlocks; // Cleared every frame

		std::vector<std::shared_ptr<re::ParameterBlock >> m_permanentParamBlocks;

		std::vector<re::Batch> m_stageBatches;
		uint32_t m_batchFilterMask;

		
	private:
		RenderStage() = delete;
		RenderStage(RenderStage const&) = delete;
		RenderStage(RenderStage&&) = delete;
		RenderStage& operator=(RenderStage const&) = delete;
	};


	class ComputeStage final : public virtual RenderStage
	{
	public:
		


	private:
		ComputeStage(std::string const& name, std::unique_ptr<ComputeStageParams>&&, RenderStageLifetime);
		friend class RenderStage;
	};


	inline RenderStage::RenderStageType RenderStage::GetStageType() const
	{
		return m_type;
	}


	inline RenderStage::RenderStageLifetime RenderStage::GetStageLifetime() const
	{
		return m_lifetime;
	}


	inline RenderStage::IStageParams const* RenderStage::GetStageParams() const
	{
		return m_stageParams.get();
	}


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


	inline std::vector<RenderStage::RenderStageTextureAndSamplerInput> const& RenderStage::GetTextureInputs() const
	{
		return m_textureSamplerInputs;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline RenderStage::IStageParams::~IStageParams() {}
}