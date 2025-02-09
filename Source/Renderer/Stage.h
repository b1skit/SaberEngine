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


	class Stage : public virtual core::INamedObject
	{
	public:
		static constexpr int k_noDepthTexAsInputFlag = -1;

		enum class Type
		{
			Parent, // Does not contribute batches
			Graphics,
			Compute,
			
			LibraryGraphics,
			LibraryCompute,

			FullscreenQuad, // Graphics queue
			Clear, // Graphics queue

			Invalid
		};
		static constexpr bool IsLibraryType(Type type);

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
			Type m_stageType;

			enum class LibraryType
			{
				ImGui,
			} m_type;

			LibraryStageParams(Type stageType, LibraryType type) : m_stageType(stageType), m_type(type) {}

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

			std::string ColorClearModeToStr() const
			{
				SEAssert(!m_colorClearModes.empty(), "Color clear modes not set");
				std::string result;
				for (auto const& mode : m_colorClearModes)
				{
					result += re::TextureTarget::ClearModeToCStr(mode);
				}
				return result;
			}
		};


	public:
		static std::shared_ptr<Stage> CreateParentStage(char const* name);

		static std::shared_ptr<Stage> CreateGraphicsStage(char const* name, GraphicsStageParams const&);
		static std::shared_ptr<Stage> CreateSingleFrameGraphicsStage(char const* name, GraphicsStageParams const&);

		static std::shared_ptr<Stage> CreateComputeStage(char const* name, ComputeStageParams const&);
		static std::shared_ptr<Stage> CreateSingleFrameComputeStage(char const* name, ComputeStageParams const&);

		static std::shared_ptr<Stage> CreateLibraryStage(char const* name, LibraryStageParams const&);

		static std::shared_ptr<Stage> CreateFullscreenQuadStage(char const* name, FullscreenQuadParams const&);
		static std::shared_ptr<Stage> CreateSingleFrameFullscreenQuadStage(char const* name, FullscreenQuadParams const&);

		static std::shared_ptr<Stage> CreateClearStage(
			ClearStageParams const&, std::shared_ptr<re::TextureTargetSet const> const&);
		static std::shared_ptr<Stage> CreateSingleFrameClearStage(
			ClearStageParams const&, std::shared_ptr<re::TextureTargetSet const>const&);


	public:
		virtual ~Stage() = default;

		Stage(Stage&&) noexcept = default;
		Stage& operator=(Stage&&) noexcept = default;

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
			char const* shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		void AddPermanentTextureInput(
			std::string const& shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		std::vector<re::TextureAndSamplerInput> const& GetPermanentTextureInputs() const;

		void AddSingleFrameTextureInput(
			char const* shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		void AddSingleFrameTextureInput(
			std::string const& shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		std::vector<re::TextureAndSamplerInput> const& GetSingleFrameTextureInputs() const;

		void AddPermanentRWTextureInput(
			std::string const& shaderName,
			core::InvPtr<re::Texture> const&,
			re::TextureView const&);

		std::vector<re::RWTextureInput> const& GetPermanentRWTextureInputs() const;

		void AddSingleFrameRWTextureInput(
			char const* shaderName,
			core::InvPtr<re::Texture> const&,
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
		explicit Stage(char const* name, std::unique_ptr<IStageParams>&&, Type, re::Lifetime);


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
		Stage() = delete;
		Stage(Stage const&) = delete;
		Stage& operator=(Stage const&) = delete;
	};


	//---


	class ParentStage final : public virtual Stage
	{
	public:
		// 

	private:
		ParentStage(char const* name, re::Lifetime);
		friend class Stage;
	};


	//---


	class ComputeStage final : public virtual Stage
	{
	public:
		// 

	private:
		ComputeStage(char const* name, std::unique_ptr<ComputeStageParams>&&, re::Lifetime);
		friend class Stage;
	};


	//---


	class FullscreenQuadStage final : public virtual Stage
	{
	public:
		//

	private:
		core::InvPtr<gr::MeshPrimitive> m_screenAlignedQuad;
		std::unique_ptr<re::Batch> m_fullscreenQuadBatch;

	private:
		FullscreenQuadStage(char const* name, std::unique_ptr<FullscreenQuadParams>&&, re::Lifetime);
		friend class Stage;
	};


	//---


	class ClearStage final : public virtual Stage
	{
	public:
		// 

	private:
		ClearStage(char const* name, re::Lifetime);
		friend class Stage;
	};


	//---


	class LibraryStage final : public virtual Stage
	{
	public:
		struct IPayload
		{
			virtual ~IPayload() = default;
		};

	public:
		void Execute(void* platformObject); // e.g. platformObject == DX12 command list
		
		// The payload is an arbitrary data blob passed by a graphics system every frame for consumption by the backend
		void SetPayload(std::unique_ptr<IPayload>&&);
		std::unique_ptr<IPayload> TakePayload();

	private:
		std::unique_ptr<IPayload> m_payload;

	private:
		LibraryStage(char const* name, std::unique_ptr<LibraryStageParams>&&, re::Lifetime);
		friend class Stage;
	};


	//---


	inline constexpr bool Stage::IsLibraryType(Type type)
	{
		return type == Type::LibraryGraphics || type == Type::LibraryCompute;
	}


	inline Stage::Type Stage::GetStageType() const
	{
		return m_type;
	}


	inline re::Lifetime Stage::GetStageLifetime() const
	{
		return m_lifetime;
	}


	inline Stage::IStageParams const* Stage::GetStageParams() const
	{
		return m_stageParams.get();
	}


	inline void Stage::SetDrawStyle(effect::drawstyle::Bitmask drawStyleBits)
	{
		m_drawStyleBits |= drawStyleBits;
	}


	inline void Stage::ClearDrawStyle()
	{
		m_drawStyleBits = 0;
	}


	inline re::TextureTargetSet const* Stage::GetTextureTargetSet() const
	{
		return m_textureTargetSet.get();
	}


	inline re::TextureTargetSet* Stage::GetTextureTargetSet()
	{
		return m_textureTargetSet.get();
	}


	inline std::vector<re::TextureAndSamplerInput> const& Stage::GetPermanentTextureInputs() const
	{
		return m_permanentTextureSamplerInputs;
	}


	inline std::vector<re::TextureAndSamplerInput> const& Stage::GetSingleFrameTextureInputs() const
	{
		return m_singleFrameTextureSamplerInputs;
	}


	inline std::vector<re::RWTextureInput> const& Stage::GetPermanentRWTextureInputs() const
	{
		return m_permanentRWTextureInputs;
	}


	inline std::vector<re::RWTextureInput> const& Stage::GetSingleFrameRWTextureInputs() const
	{
		return m_singleFrameRWTextureInputs;
	}


	inline bool Stage::DepthTargetIsAlsoTextureInput() const
	{
		return m_depthTextureInputIdx != k_noDepthTexAsInputFlag;
	}


	inline int Stage::GetDepthTargetTextureInputIdx() const
	{
		return m_depthTextureInputIdx;
	}


	inline std::vector<re::BufferInput> const& Stage::GetPermanentBuffers() const
	{
		return m_permanentBuffers;
	}


	inline std::vector<re::BufferInput> const& Stage::GetPerFrameBuffers() const
	{
		return m_singleFrameBuffers;
	}


	inline std::vector<re::Batch> const& Stage::GetStageBatches() const
	{
		return m_stageBatches;
	}
}