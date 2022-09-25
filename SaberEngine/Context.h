#pragma once

#include <memory>

#include "Context_Platform.h"


namespace re
{
	class Context
	{
	public:
		Context();

		platform::Context::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		platform::Context::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }

		// Platform wrappers:
		void Create();
		void Destroy();

		void SwapWindow() const;
		void SetCullingMode(platform::Context::FaceCullingMode const& mode) const;
		void ClearTargets(platform::Context::ClearTarget const& clearTarget) const;
		void SetBlendMode(platform::Context::BlendMode const& src, platform::Context::BlendMode const& dst) const;
		void SetDepthTestMode(platform::Context::DepthTestMode const& mode) const;
		void SetDepthWriteMode(platform::Context::DepthWriteMode const& mode) const;
		void SetColorWriteMode(platform::Context::ColorWriteMode const& channelModes) const;

		// Static platform wrappers:
		static uint32_t GetMaxTextureInputs();

	private:
		std::unique_ptr<platform::Context::PlatformParams> m_platformParams;

		// Friends:
		friend void platform::Context::PlatformParams::CreatePlatformParams(re::Context& m_context);
	};
}