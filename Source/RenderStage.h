// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Batch.h"
#include "Camera.h"
#include "Context_Platform.h"
#include "NamedObject.h"
#include "ParameterBlock.h"
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
		static constexpr int k_noDepthTexAsInputFlag = -1;

		enum class Lifetime
		{
			SingleFrame,
			Permanent
		};
		enum class Type
		{
			Parent, // Does not contribute batches
			Graphics,
			Compute,

			Clear, // Graphics queue

			Invalid
			// TODO: Add specialist types: Fullscreen, Parent, Clear etc
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
		struct ClearStageParams final : public virtual IStageParams
		{
			// 1 entry: applied to all targets, or per-target if m_colorClearMode.size() == targetSet.GetNumColorTargets()
			std::vector<re::TextureTarget::TargetParams::ClearMode> m_colorClearModes;
			glm::vec4 m_clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);

			re::TextureTarget::TargetParams::ClearMode m_depthClearMode = 
				re::TextureTarget::TargetParams::ClearMode::Disabled;
			float m_clearDepth = 1.f; // Far plane
		};

		struct RenderStageTextureAndSamplerInput
		{
			std::string m_shaderName;
			re::Texture const* m_texture;
			std::shared_ptr<Sampler> m_sampler;

			uint32_t m_srcMip = re::Texture::k_allMips;
		};


	public:
		static std::shared_ptr<RenderStage> CreateParentStage(std::string const& name);

		static std::shared_ptr<RenderStage> CreateGraphicsStage(std::string const& name, GraphicsStageParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameGraphicsStage(std::string const& name, GraphicsStageParams const&);

		static std::shared_ptr<RenderStage> CreateComputeStage(std::string const& name, ComputeStageParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameComputeStage(std::string const& name, ComputeStageParams const&);

		static std::shared_ptr<RenderStage> CreateClearStage(
			ClearStageParams const&, std::shared_ptr<re::TextureTargetSet const>);
		static std::shared_ptr<RenderStage> CreateSingleFrameClearStage(
			ClearStageParams const&, std::shared_ptr<re::TextureTargetSet const>);


		~RenderStage() = default;

		void EndOfFrame(); // Clears per-frame data. Called by the owning RenderPipeline

		bool IsSkippable() const;

		Type GetStageType() const;
		Lifetime GetStageLifetime() const;
		IStageParams const* GetStageParams() const;

		// TODO: The stage shader should be a member of the GraphicsStageParams/ComputeStageParams
		void SetStageShader(std::shared_ptr<re::Shader>);
		re::Shader* GetStageShader() const;

		std::shared_ptr<re::TextureTargetSet const> GetTextureTargetSet() const;
		void SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet);

		void AddTextureInput(
			std::string const& shaderName,
			re::Texture const*,
			std::shared_ptr<re::Sampler>,
			uint32_t mipLevel = re::Texture::k_allMips);
		void AddTextureInput(
			std::string const& shaderName, 
			std::shared_ptr<re::Texture>, 
			std::shared_ptr<re::Sampler>, 
			uint32_t mipLevel = re::Texture::k_allMips);
		std::vector<RenderStage::RenderStageTextureAndSamplerInput> const& GetTextureInputs() const;

		bool DepthTargetIsAlsoTextureInput() const;
		int GetDepthTargetTextureInputIdx() const;

		void AddPermanentParameterBlock(std::shared_ptr<re::ParameterBlock> pb);
		inline std::vector<std::shared_ptr<re::ParameterBlock>> const& GetPermanentParameterBlocks() const { return m_permanentParamBlocks; }
		
		void AddSingleFrameParameterBlock(std::shared_ptr<re::ParameterBlock> pb);
		inline std::vector<std::shared_ptr<re::ParameterBlock>> const& GetPerFrameParameterBlocks() const { return m_singleFrameParamBlocks; }

		// Stage Batches:
		std::vector<re::Batch> const& GetStageBatches() const;
		void AddBatches(std::vector<re::Batch> const& batches);
		void AddBatch(re::Batch const& batch);

		inline uint32_t GetBatchFilterMask() const { return m_batchFilterBitmask; }
		void SetBatchFilterMaskBit(re::Batch::Filter filterBit);


	protected:
		explicit RenderStage(std::string const& name, std::unique_ptr<IStageParams>&&, Type, Lifetime);


	private:
		void UpdateDepthTextureInputIndex();
		void ValidateTexturesAndTargets();
		

	private:
		const Type m_type;
		const Lifetime m_lifetime;
		std::unique_ptr<IStageParams> m_stageParams;

		std::shared_ptr<re::Shader> m_stageShader;

		std::shared_ptr<re::TextureTargetSet> m_textureTargetSet;
		std::vector<RenderStageTextureAndSamplerInput> m_textureSamplerInputs;
		int m_depthTextureInputIdx; // k_noDepthTexAsInputFlag: Depth not attached as an input		

		std::vector<std::shared_ptr<re::ParameterBlock>> m_singleFrameParamBlocks; // Cleared every frame

		std::vector<std::shared_ptr<re::ParameterBlock >> m_permanentParamBlocks;

		std::vector<re::Batch> m_stageBatches;
		uint32_t m_batchFilterBitmask;

		
	private:
		RenderStage() = delete;
		RenderStage(RenderStage const&) = delete;
		RenderStage(RenderStage&&) = delete;
		RenderStage& operator=(RenderStage const&) = delete;
	};


	class ParentStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ParentStage(std::string const& name, Lifetime);
		friend class RenderStage;
	};


	class ComputeStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ComputeStage(std::string const& name, std::unique_ptr<ComputeStageParams>&&, Lifetime);
		friend class RenderStage;
	};


	class ClearStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ClearStage(std::string const& name, Lifetime);
		friend class RenderStage;
	};


	inline RenderStage::Type RenderStage::GetStageType() const
	{
		return m_type;
	}


	inline RenderStage::Lifetime RenderStage::GetStageLifetime() const
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


	inline bool RenderStage::DepthTargetIsAlsoTextureInput() const
	{
		return m_depthTextureInputIdx != k_noDepthTexAsInputFlag;
	}


	inline int RenderStage::GetDepthTargetTextureInputIdx() const
	{
		return m_depthTextureInputIdx;
	}


	inline std::vector<re::Batch> const& RenderStage::GetStageBatches() const
	{
		return m_stageBatches;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline RenderStage::IStageParams::~IStageParams() {}
}