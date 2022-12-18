#pragma once

#include <memory>

#include "DebugConfiguration.h"
#include "Texture.h"
#include "NamedObject.h"
#include "ParameterBlock.h"


namespace re
{
	// Wrapper for an individual render target texture
	class TextureTarget
	{
	public:
		struct PlatformParams
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
		struct PlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_colorIsCreated = false;
			bool m_depthIsCreated = false;
		};


	public:
		struct TargetParams
		{
			glm::vec4 g_targetResolution = glm::vec4(0.f);
		};


	public:
		explicit TextureTargetSet(std::string name);
		TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName);
		TextureTargetSet(TextureTargetSet&&) = default;
		TextureTargetSet& operator=(TextureTargetSet const&);
		~TextureTargetSet();

		inline std::vector<re::TextureTarget> const& ColorTargets() const { return m_colorTargets; }
		re::TextureTarget const& GetColorTarget(size_t i) const;
		void SetColorTarget(size_t i, re::TextureTarget texTarget);
		void SetColorTarget(size_t i, std::shared_ptr<re::Texture> texTarget);

		inline re::TextureTarget const& DepthStencilTarget() const { return m_depthStencilTarget; }
		void SetDepthStencilTarget(re::TextureTarget const& depthStencilTarget);
		void SetDepthStencilTarget(std::shared_ptr<re::Texture> depthStencilTarget);

		bool HasTargets();
		bool HasColorTarget();
		bool HasDepthTarget();

		inline re::Viewport& Viewport() { return m_viewport; }
		inline re::Viewport const& Viewport() const { return m_viewport; }

		inline PlatformParams* GetPlatformParams() { return m_platformParams.get(); }
		inline PlatformParams const* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::shared_ptr<PlatformParams> params) { m_platformParams = params; }
		
		std::shared_ptr<re::ParameterBlock> GetTargetParameterBlock();


	private:
		void UpdateTargetParameterBlock();


	private:
		std::vector<re::TextureTarget> m_colorTargets;
		re::TextureTarget m_depthStencilTarget;

		bool m_hasColorTarget;
		bool m_colorTargetStateDirty;

		re::Viewport m_viewport;

		std::shared_ptr<PlatformParams> m_platformParams;

		std::shared_ptr<re::ParameterBlock> m_targetParameterBlock;
		bool m_targetParamsDirty; // Do we need to recompute the target parameter block?


	private:
		TextureTargetSet() = delete;
		TextureTargetSet(TextureTargetSet const&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline TextureTarget::PlatformParams::~PlatformParams() {};
	inline TextureTargetSet::PlatformParams::~PlatformParams() {};
}
