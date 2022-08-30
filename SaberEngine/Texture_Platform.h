#pragma once

#include <memory>

namespace gr
{
	class Texture;
}

namespace platform
{
	class Texture
	{
	public:
		enum class UVOrigin
		{
			BottomLeft,	// OpenGL style
			TopLeft,	// D3D style

			Invalid,
			PlatformTextureUVOriginCount = Invalid
		};


		struct PlatformParams
		{
			// Params contain unique GPU bindings that should not be arbitrarily copied/duplicated
			PlatformParams() = default;
			PlatformParams(PlatformParams&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams&) = delete;
			PlatformParams& operator=(PlatformParams&&) = delete;

			// API-specific GPU bindings should be destroyed here
			virtual ~PlatformParams() = 0;

			static void CreatePlatformParams(gr::Texture& texture);
		};


		// Static members:
		/*****************/
		static void (*Create)(gr::Texture&, uint32_t textureUnit); // Create a texture + sampler (leaving it bound)
		static void (*Bind)(gr::Texture const&, uint32_t textureUnit, bool doBind);
		static void (*Destroy)(gr::Texture&);

		static void (*GenerateMipMaps)(gr::Texture&);
		static UVOrigin (*GetUVOrigin)();
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline platform::Texture::PlatformParams::~PlatformParams() {};
}

