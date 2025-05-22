// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "SwapChain.h"
#include "Texture.h"


namespace re
{
	class TextureTargetSet;
}

namespace opengl
{
	class SwapChain
	{
	public:
		struct PlatObj final : public re::SwapChain::PlatObj
		{
			// OpenGL manages the swap chain implicitly. We just maintain a single target set representing the default
			// framebuffer instead
			std::shared_ptr<re::TextureTargetSet> m_backbufferTargetSet;

			glm::uvec2 m_backbufferDimensions = glm::uvec2(0, 0);
			re::Texture::Format m_backbufferFormat = re::Texture::Format::Invalid;			
		};


	public:
		static void Create(re::SwapChain&, re::Texture::Format);
		static void Destroy(re::SwapChain&);
		static bool ToggleVSync(re::SwapChain const&);

		static std::shared_ptr<re::TextureTargetSet> GetBackBufferTargetSet(re::SwapChain const&);
		static re::Texture::Format GetBackbufferFormat(re::SwapChain const&);
		static glm::uvec2 GetBackbufferDimensions(re::SwapChain const&);
	};
}