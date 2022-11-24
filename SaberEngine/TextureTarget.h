#pragma once

#include <memory>

#include "DebugConfiguration.h"
#include "Texture.h"
#include "TextureTarget_Platform.h"
#include "NamedObject.h"
#include "ParameterBlock.h"


namespace re
{
	// Wrapper for an individual render target texture
	class TextureTarget
	{
	public:
		TextureTarget();
		explicit TextureTarget(std::shared_ptr<gr::Texture> texture);
		
		~TextureTarget();

		TextureTarget(TextureTarget const&) = default;
		TextureTarget(TextureTarget&&) = default;

		TextureTarget& operator=(TextureTarget const&) = default;
		TextureTarget& operator=(std::shared_ptr<gr::Texture> texture);

		std::shared_ptr<gr::Texture>& GetTexture() { return m_texture; }
		std::shared_ptr<gr::Texture> const& GetTexture() const { return m_texture; }

		platform::TextureTarget::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		platform::TextureTarget::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }

	private:
		std::shared_ptr<gr::Texture> m_texture;
		std::shared_ptr<platform::TextureTarget::PlatformParams> m_platformParams;

		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::TextureTarget::PlatformParams::CreatePlatformParams(re::TextureTarget&);
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

		uint32_t& xMin() { return m_xMin; }
		uint32_t const& xMin() const{ return m_xMin; }

		uint32_t& yMin() { return m_yMin; }
		uint32_t const& yMin() const { return m_yMin; }

		uint32_t& Width() { return m_width; }
		uint32_t const& Width() const { return m_width; }

		uint32_t& Height() { return m_height; }
		uint32_t const& Height() const { return m_height; }

		
	private:
		// Viewport origin pixel coordinates. (0,0) by default
		uint32_t m_xMin;
		uint32_t m_yMin;

		// Viewport dimensions. Full window resolution by default
		uint32_t m_width;
		uint32_t m_height;
	};


	// Collection of render target textures
	class TextureTargetSet : public virtual en::NamedObject
	{
	public:
		struct TargetParams
		{
			glm::vec4 g_targetResolution;
		};

	public:
		explicit TextureTargetSet(std::string name);
		TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName);
		TextureTargetSet(TextureTargetSet&&) = default;
		TextureTargetSet& operator=(TextureTargetSet const&);
		~TextureTargetSet();

		inline std::vector<re::TextureTarget>& ColorTargets() { m_targetStateDirty = true; return m_colorTargets; }
		inline std::vector<re::TextureTarget> const& ColorTargets() const { return m_colorTargets; }

		inline re::TextureTarget& ColorTarget(size_t i)
			{ SEAssert("OOB index", i < m_colorTargets.size()); m_targetStateDirty = true; return m_colorTargets[i]; }
		inline re::TextureTarget const& ColorTarget(size_t i) const 
			{ SEAssert("OOB index", i < m_colorTargets.size()); return m_colorTargets[i]; }

		inline re::TextureTarget& DepthStencilTarget() { m_targetStateDirty = true; return m_depthStencilTarget; }
		inline re::TextureTarget const& DepthStencilTarget() const { return m_depthStencilTarget; }

		inline re::Viewport& Viewport() { return m_viewport; }
		inline re::Viewport const& Viewport() const { return m_viewport; }

		inline platform::TextureTargetSet::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		inline platform::TextureTargetSet::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }
		
		inline std::shared_ptr<re::ParameterBlock> GetTargetParameterBlock() const { return m_targetParameterBlock; }

		bool HasTargets();

		// Platform wrappers:
		void CreateColorTargets();
		void AttachColorTargets(uint32_t face, uint32_t mipLevel, bool doBind) const;

		void CreateDepthStencilTarget();
		void AttachDepthStencilTarget(bool doBind) const;

		void CreateColorDepthStencilTargets();
		void AttachColorDepthStencilTargets(uint32_t colorFace, uint32_t colorMipLevel, bool doBind) const;
		
	private:
		std::vector<re::TextureTarget> m_colorTargets;
		re::TextureTarget m_depthStencilTarget;
		bool m_targetStateDirty;
		bool m_hasTargets;

		re::Viewport m_viewport;

		std::shared_ptr<platform::TextureTargetSet::PlatformParams> m_platformParams;

		std::shared_ptr<re::ParameterBlock> m_targetParameterBlock;

		bool m_colorIsCreated;
		bool m_depthIsCreated;

	private:
		void CreateUpdateTargetParameterBlock();

	private:
		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::TextureTargetSet::PlatformParams::CreatePlatformParams(re::TextureTargetSet&);

		TextureTargetSet() = delete;
		TextureTargetSet(TextureTargetSet const&) = delete;
	};
}
