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
		struct PlatformParams : public re::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
		};

	public:
		struct TargetParams
		{
			glm::vec4 m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

			// Subresource info:
			uint32_t m_targetFace = 0;
			uint32_t m_targetSubesource = 0;

			// TODO: Support additional target/sub-resource parameters:
			// - Array index (or first index, and offset from that)
			// - Array size
			// - Texture view mode: linear, sRGB

			// - Read/write flags: depth, stencil, RGBA ?
			// - Clear mode, clear color/values?
		};

	public:
		TextureTarget() = default;
		explicit TextureTarget(std::shared_ptr<re::Texture> texture, TargetParams const&);
		
		~TextureTarget();

		TextureTarget(TextureTarget const&) = default;
		TextureTarget(TextureTarget&&) = default;
		TextureTarget& operator=(TextureTarget const&) = default;

		inline bool HasTexture() const { return m_texture != nullptr; }

		std::shared_ptr<re::Texture>& GetTexture() { return m_texture; }
		std::shared_ptr<re::Texture> const& GetTexture() const { return m_texture; }

		void SetTargetParams(TargetParams const& targetParams) { m_targetParams = targetParams; }
		TargetParams const& GetTargetParams() const { return m_targetParams; }

		PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::shared_ptr<PlatformParams> params) { m_platformParams = params; }

	private:
		std::shared_ptr<re::Texture> m_texture;
		std::shared_ptr<PlatformParams> m_platformParams;

		TargetParams m_targetParams;
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

	private: // ScissorRect bounds, in pixel coordinates: 
		long m_left;
		long m_top;
		long m_right;
		long m_bottom;
	};


	// Collection of render target textures
	class TextureTargetSet final : public en::NamedObject, public en::HashedDataObject
	{
	public:
		struct PlatformParams : public re::IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			// Target sets are immutable after Commit
			bool m_colorIsCreated = false;
			bool m_depthIsCreated = false;
		};


	public:
		static std::shared_ptr<re::TextureTargetSet> Create(std::string const& name);
		static std::shared_ptr<re::TextureTargetSet> Create(TextureTargetSet const&, std::string const& name);

		~TextureTargetSet();

		void Commit(); // Target sets are immutable after Commit: Called once during API creation

		inline std::vector<re::TextureTarget> const& GetColorTargets() const { return m_colorTargets; }
		re::TextureTarget const* GetColorTarget(uint8_t slot) const;
		void SetColorTarget(uint8_t slot, re::TextureTarget const* texTarget);
		void SetColorTarget(uint8_t slot, std::shared_ptr<re::Texture> texTarget, TextureTarget::TargetParams const&);

		re::TextureTarget const* GetDepthStencilTarget() const;
		void SetDepthStencilTarget(re::TextureTarget const* depthStencilTarget);
		void SetDepthStencilTarget(std::shared_ptr<re::Texture> depthStencilTarget, re::TextureTarget::TargetParams const&);

		bool HasTargets() const;
		bool HasColorTarget() const;
		bool HasDepthTarget() const;

		uint8_t GetNumColorTargets() const;
		glm::vec4 GetTargetDimensions() const;

		inline re::Viewport& Viewport() { return m_viewport; }
		inline re::Viewport const& Viewport() const { return m_viewport; }

		inline re::ScissorRect& ScissorRect() { return m_scissorRect; }
		inline re::ScissorRect const& ScissorRect() const { return m_scissorRect; }

		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::shared_ptr<PlatformParams> params) { m_platformParams = params; }
		
		uint64_t GetTargetSetSignature(); // Use this instead of HashedDataObject::GetDataHash
		uint64_t GetTargetSetSignature() const;

	private: // Use the object Create factories instead
		explicit TextureTargetSet(std::string const& name);
		TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName);


	private:
		void RecomputeNumColorTargets();
		void ComputeDataHash() override; // HashedDataObject interface


	private:
		std::vector<re::TextureTarget> m_colorTargets; // == SysInfo::GetMaxRenderTargets() elements
		re::TextureTarget m_depthStencilTarget;

		uint8_t m_numColorTargets;

		re::Viewport m_viewport;
		re::ScissorRect m_scissorRect;

		std::shared_ptr<PlatformParams> m_platformParams;


	private:
		TextureTargetSet() = delete;
		TextureTargetSet(TextureTargetSet const&) = delete;
		TextureTargetSet(TextureTargetSet&&) = delete;
		TextureTargetSet& operator=(TextureTargetSet const&) = delete;
		TextureTargetSet& operator=(TextureTargetSet&&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline TextureTarget::PlatformParams::~PlatformParams() {};
	inline TextureTargetSet::PlatformParams::~PlatformParams() {};
}
