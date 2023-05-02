// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "DebugConfiguration.h"
#include "HashedDataObject.h"
#include "IPlatformParams.h"
#include "Texture.h"
#include "NamedObject.h"
#include "ParameterBlock.h"


namespace re
{
	// Wrapper for an individual render target texture
	class TextureTarget
	{
	public:
		struct PlatformParams : public IPlatformParams
		{
			virtual ~PlatformParams() = 0;
		};


	public:
		TextureTarget();
		explicit TextureTarget(std::shared_ptr<re::Texture> texture);
		
		~TextureTarget();

		TextureTarget(TextureTarget const&) = default;
		TextureTarget(TextureTarget&&) = default;
		TextureTarget& operator=(TextureTarget const&) = default;

		TextureTarget& operator=(std::shared_ptr<re::Texture> texture);

		inline bool HasTexture() const { return m_texture != nullptr; }

		std::shared_ptr<re::Texture>& GetTexture() { return m_texture; }
		std::shared_ptr<re::Texture> const& GetTexture() const { return m_texture; }

		PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::shared_ptr<PlatformParams> params) { m_platformParams = params; }

	private:
		std::shared_ptr<re::Texture> m_texture;
		std::shared_ptr<PlatformParams> m_platformParams;
	};


	class Viewport
	{
	public:
		Viewport();
		Viewport(uint32_t xMin, uint32_t yMin, uint32_t width, uint32_t height);

		Viewport(Viewport const&) = default;
		Viewport(Viewport&&) = default;
		Viewport& operator=(Viewport const&) = default;
		~Viewport() = default;

		uint32_t& xMin() { return m_xMin; } // Top-left X
		uint32_t const xMin() const{ return m_xMin; }

		uint32_t& yMin() { return m_yMin; } // Top-left Y
		uint32_t const yMin() const { return m_yMin; }

		uint32_t& Width() { return m_width; }
		uint32_t const Width() const { return m_width; }

		uint32_t& Height() { return m_height; }
		uint32_t const Height() const { return m_height; }

		
	private:
		// Viewport origin pixel coordinates. (0,0) (top-left) by default
		uint32_t m_xMin;
		uint32_t m_yMin;

		// Viewport dimensions. Full window resolution by default
		uint32_t m_width;
		uint32_t m_height;
	};


	class ScissorRect
	{
	public:
		ScissorRect();
		ScissorRect(long left, long top, long right, long bottom);

		long& Left() { return m_left; }
		long Left() const { return m_left; }

		long& Top() { return m_top; }
		long Top() const { return m_top; }

		long& Right() { return m_right; }
		long Right() const { return m_right; }

		long& Bottom() { return m_bottom; }
		long Bottom() const { return m_bottom; }

	private:
		long m_left;
		long m_top;
		long m_right;
		long m_bottom;

		// TODO: For some reason, D3D12 doesn't render if right/bottom are not std::numeric_limits<long>::max(). Why?
	};


	// Collection of render target textures
	class TextureTargetSet final : public en::NamedObject, public en::HashedDataObject
	{
	public:
		struct PlatformParams : public IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_colorIsCreated = false;
			bool m_depthIsCreated = false;
		};


	public:
		struct RenderTargetParams
		{
			glm::vec4 g_targetResolution = glm::vec4(0.f);

			static constexpr char const* const s_shaderName = "RenderTargetParams"; // Not counted towards size of struct
		};


	public:
		explicit TextureTargetSet(std::string const& name);
		TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName);
		TextureTargetSet(TextureTargetSet&&) = default;
		TextureTargetSet& operator=(TextureTargetSet const&);
		~TextureTargetSet();

		inline std::vector<re::TextureTarget> const& GetColorTargets() const { return m_colorTargets; }
		re::TextureTarget const& GetColorTarget(uint8_t slot) const;
		void SetColorTarget(uint8_t slot, re::TextureTarget texTarget);
		void SetColorTarget(uint8_t slot, std::shared_ptr<re::Texture> texTarget);

		inline re::TextureTarget const& GetDepthStencilTarget() const { return m_depthStencilTarget; }
		void SetDepthStencilTarget(re::TextureTarget const& depthStencilTarget);
		void SetDepthStencilTarget(std::shared_ptr<re::Texture> depthStencilTarget);

		bool HasTargets();
		bool HasColorTarget();
		bool HasDepthTarget();

		uint8_t GetNumColorTargets();

		inline re::Viewport& Viewport() { return m_viewport; }
		inline re::Viewport const& Viewport() const { return m_viewport; }

		inline re::ScissorRect& ScissorRect() { return m_scissorRect; }
		inline re::ScissorRect const& ScissorRect() const { return m_scissorRect; }

		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::shared_ptr<PlatformParams> params) { m_platformParams = params; }
		
		std::shared_ptr<re::ParameterBlock> GetTargetParameterBlock();

		uint64_t GetTargetSetSignature(); // Use this instead of HashedDataObject::GetDataHash

	private: // Internal state tracking:
		void RecomputeInternalState();

		void RecomputeNumColorTargets();
		void RecomputeTargetParameterBlock();
		void ComputeDataHash() override; // HashedDataObject interface


	private:
		std::vector<re::TextureTarget> m_colorTargets;
		re::TextureTarget m_depthStencilTarget;

		uint8_t m_numColorTargets;
		bool m_targetStateDirty;

		re::Viewport m_viewport;
		re::ScissorRect m_scissorRect; // TODO: Use this in OpenGL

		std::shared_ptr<PlatformParams> m_platformParams;

		std::shared_ptr<re::ParameterBlock> m_targetParameterBlock;


	private:
		TextureTargetSet() = delete;
		TextureTargetSet(TextureTargetSet const&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline TextureTarget::PlatformParams::~PlatformParams() {};
	inline TextureTargetSet::PlatformParams::~PlatformParams() {};
}
