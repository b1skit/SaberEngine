// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "Effect.h"
#include "MeshFactory.h"
#include "TextureView.h"
#include "TextureTarget.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"


namespace re
{
	class ComputeStage;
	class Buffer;
	class BufferInput;
	class Shader;
	class Texture;


	class RenderStage : public virtual core::INamedObject
	{
	public:
		static constexpr int k_noDepthTexAsInputFlag = -1;

		enum class Type
		{
			Parent, // Does not contribute batches
			Graphics,
			Compute,
			
			Library, // Wrapper for external libraries

			FullscreenQuad, // Graphics queue
			Clear, // Graphics queue

			Invalid
		};
		struct IStageParams
		{
			virtual ~IStageParams() = default;
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
		struct LibraryStageParams final : public virtual IStageParams
		{
			enum class LibraryType
			{
				ImGui,
			} m_type;

			LibraryStageParams(LibraryType type) : m_type(type) {}

			std::shared_ptr<void> m_payload; // Interpreted by the library wrapper

		private:
			LibraryStageParams() = delete;
		};
		struct FullscreenQuadParams final : public virtual IStageParams
		{
			gr::meshfactory::ZLocation m_zLocation = gr::meshfactory::ZLocation::Near;

			EffectID m_effectID;
			effect::drawstyle::Bitmask m_drawStyleBitmask = effect::drawstyle::DefaultTechnique;
		};
		struct ClearStageParams final : public virtual IStageParams
		{
			// 1 entry: applied to all targets, or per-target if m_colorClearMode.size() == targetSet.GetNumColorTargets()
			std::vector<re::TextureTarget::ClearMode> m_colorClearModes;
			glm::vec4 m_clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);

			re::TextureTarget::ClearMode m_depthClearMode = 
				re::TextureTarget::ClearMode::Disabled;
			float m_clearDepth = 1.f; // Far plane
		};


	public:
		static std::shared_ptr<RenderStage> CreateParentStage(char const* name);

		static std::shared_ptr<RenderStage> CreateGraphicsStage(char const* name, GraphicsStageParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameGraphicsStage(char const* name, GraphicsStageParams const&);

		static std::shared_ptr<RenderStage> CreateComputeStage(char const* name, ComputeStageParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameComputeStage(char const* name, ComputeStageParams const&);

		static std::shared_ptr<RenderStage> CreateLibraryStage(char const* name, LibraryStageParams const&);

		static std::shared_ptr<RenderStage> CreateFullscreenQuadStage(char const* name, FullscreenQuadParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameFullscreenQuadStage(char const* name, FullscreenQuadParams const&);

		static std::shared_ptr<RenderStage> CreateClearStage(
			ClearStageParams const&, std::shared_ptr<re::TextureTargetSet const>);
		static std::shared_ptr<RenderStage> CreateSingleFrameClearStage(
			ClearStageParams const&, std::shared_ptr<re::TextureTargetSet const>);


	public:
		~RenderStage() = default;

		RenderStage(RenderStage&&) noexcept = default;
		RenderStage& operator=(RenderStage&&) noexcept = default;

		void PostUpdatePreRender();
		void EndOfFrame(); // Clears per-frame data. Called by the owning RenderPipeline

		bool IsSkippable() const;

		Type GetStageType() const;
		re::Lifetime GetStageLifetime() const;
		IStageParams const* GetStageParams() const;

		void SetDrawStyle(effect::drawstyle::Bitmask);
		void ClearDrawStyle();

		re::TextureTargetSet const* GetTextureTargetSet() const;
		re::TextureTargetSet* GetTextureTargetSet();
		void SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> const& targetSet);

		void AddPermanentTextureInput(
			std::string const& shaderName,
			re::Texture const*,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		void AddPermanentTextureInput(
			std::string const& shaderName,
			std::shared_ptr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		std::vector<re::TextureAndSamplerInput> const& GetPermanentTextureInputs() const;

		void AddSingleFrameTextureInput(
			char const* shaderName,
			re::Texture const*,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		void AddSingleFrameTextureInput(
			char const* shaderName,
			std::shared_ptr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		std::vector<re::TextureAndSamplerInput> const& GetSingleFrameTextureInputs() const;

		void AddPermanentRWTextureInput(
			std::string const& shaderName,
			re::Texture const*,
			re::TextureView const&);

		void AddPermanentRWTextureInput(
			std::string const& shaderName,
			std::shared_ptr<re::Texture> const&,
			re::TextureView const&);

		std::vector<re::RWTextureInput> const& GetPermanentRWTextureInputs() const;

		void AddSingleFrameRWTextureInput(
			char const* shaderName,
			re::Texture const*,
			re::TextureView const&);

		void AddSingleFrameRWTextureInput(
			char const* shaderName,
			std::shared_ptr<re::Texture> const&,
			re::TextureView const&);

		std::vector<re::RWTextureInput> const& GetSingleFrameRWTextureInputs() const;

		bool DepthTargetIsAlsoTextureInput() const;
		int GetDepthTargetTextureInputIdx() const;

		void AddPermanentBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&); // Infer a default BufferView
		void AddPermanentBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		void AddPermanentBuffer(re::BufferInput const&);
		void AddPermanentBuffer(re::BufferInput&&);
		inline std::vector<re::BufferInput> const& GetPermanentBuffers() const;
		
		void AddSingleFrameBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&); // Infer a default BufferView
		void AddSingleFrameBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		void AddSingleFrameBuffer(re::BufferInput const&);
		void AddSingleFrameBuffer(re::BufferInput&&);
		inline std::vector<re::BufferInput> const& GetPerFrameBuffers() const;

		// Stage Batches:
		std::vector<re::Batch> const& GetStageBatches() const;
		void AddBatches(std::vector<re::Batch> const&);
		re::Batch* AddBatch(re::Batch const&); // Returns Batch ptr (IFF it was successfully added)
		re::Batch* AddBatchWithLifetime(re::Batch const&, re::Lifetime);

		enum class FilterMode
		{
			Require,
			Exclude
		};
		void SetBatchFilterMaskBit(re::Batch::Filter filterBit, FilterMode, bool enabled);


	protected:
		explicit RenderStage(char const* name, std::unique_ptr<IStageParams>&&, Type, re::Lifetime);


	private:
		void UpdateDepthTextureInputIndex();
		void ValidateTexturesAndTargets();
		

	protected:
		const Type m_type;
		const re::Lifetime m_lifetime;
		std::unique_ptr<IStageParams> m_stageParams;

		std::shared_ptr<re::TextureTargetSet> m_textureTargetSet;
		std::vector<re::TextureAndSamplerInput> m_permanentTextureSamplerInputs;
		std::vector<re::TextureAndSamplerInput> m_singleFrameTextureSamplerInputs;
		int m_depthTextureInputIdx; // k_noDepthTexAsInputFlag: Depth not attached as an input		

		std::vector<re::RWTextureInput> m_permanentRWTextureInputs;
		std::vector<re::RWTextureInput> m_singleFrameRWTextureInputs;

		std::vector<re::BufferInput> m_singleFrameBuffers; // Cleared every frame

		std::vector<re::BufferInput> m_permanentBuffers;

		std::vector<re::Batch> m_stageBatches;

		re::Batch::FilterBitmask m_requiredBatchFilterBitmasks;
		re::Batch::FilterBitmask m_excludedBatchFilterBitmasks;

		effect::drawstyle::Bitmask m_drawStyleBits;

		
	private:
		RenderStage() = delete;
		RenderStage(RenderStage const&) = delete;
		RenderStage& operator=(RenderStage const&) = delete;
	};


	//---


	class ParentStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ParentStage(char const* name, re::Lifetime);
		friend class RenderStage;
	};


	//---


	class ComputeStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ComputeStage(char const* name, std::unique_ptr<ComputeStageParams>&&, re::Lifetime);
		friend class RenderStage;
	};


	//---


	class FullscreenQuadStage final : public virtual RenderStage
	{
	public:
		//

	private:
		std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
		std::unique_ptr<re::Batch> m_fullscreenQuadBatch;

	private:
		FullscreenQuadStage(char const* name, std::unique_ptr<FullscreenQuadParams>&&, re::Lifetime);
		friend class RenderStage;
	};


	//---


	class ClearStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ClearStage(char const* name, re::Lifetime);
		friend class RenderStage;
	};


	//---


	class LibraryStage final : public virtual RenderStage
	{
	public:
		struct IPayload
		{
			virtual ~IPayload() = default;
		};

	public:
		void Execute();
		
		// The payload is an arbitrary data blob passed by a graphics system every frame for consumption by the backend
		void SetPayload(std::unique_ptr<IPayload>&&);
		std::unique_ptr<IPayload> TakePayload();

	private:
		std::unique_ptr<IPayload> m_payload;

	private:
		LibraryStage(char const* name, std::unique_ptr<LibraryStageParams>&&, re::Lifetime);
		friend class RenderStage;
	};


	//---


	inline RenderStage::Type RenderStage::GetStageType() const
	{
		return m_type;
	}


	inline re::Lifetime RenderStage::GetStageLifetime() const
	{
		return m_lifetime;
	}


	inline RenderStage::IStageParams const* RenderStage::GetStageParams() const
	{
		return m_stageParams.get();
	}


	inline void RenderStage::SetDrawStyle(effect::drawstyle::Bitmask drawStyleBits)
	{
		m_drawStyleBits |= drawStyleBits;
	}


	inline void RenderStage::ClearDrawStyle()
	{
		m_drawStyleBits = 0;
	}


	inline re::TextureTargetSet const* RenderStage::GetTextureTargetSet() const
	{
		return m_textureTargetSet.get();
	}


	inline re::TextureTargetSet* RenderStage::GetTextureTargetSet()
	{
		return m_textureTargetSet.get();
	}


	inline std::vector<re::TextureAndSamplerInput> const& RenderStage::GetPermanentTextureInputs() const
	{
		return m_permanentTextureSamplerInputs;
	}


	inline std::vector<re::TextureAndSamplerInput> const& RenderStage::GetSingleFrameTextureInputs() const
	{
		return m_singleFrameTextureSamplerInputs;
	}


	inline std::vector<re::RWTextureInput> const& RenderStage::GetPermanentRWTextureInputs() const
	{
		return m_permanentRWTextureInputs;
	}


	inline std::vector<re::RWTextureInput> const& RenderStage::GetSingleFrameRWTextureInputs() const
	{
		return m_singleFrameRWTextureInputs;
	}


	inline bool RenderStage::DepthTargetIsAlsoTextureInput() const
	{
		return m_depthTextureInputIdx != k_noDepthTexAsInputFlag;
	}


	inline int RenderStage::GetDepthTargetTextureInputIdx() const
	{
		return m_depthTextureInputIdx;
	}


	inline std::vector<re::BufferInput> const& RenderStage::GetPermanentBuffers() const
	{
		return m_permanentBuffers;
	}


	inline std::vector<re::BufferInput> const& RenderStage::GetPerFrameBuffers() const
	{
		return m_singleFrameBuffers;
	}


	inline std::vector<re::Batch> const& RenderStage::GetStageBatches() const
	{
		return m_stageBatches;
	}
}