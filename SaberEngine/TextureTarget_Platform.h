#pragma once

#include <memory>


namespace gr
{
	class Texture;
	class TextureTarget;
	class TextureTargetSet;
}

namespace platform
{
	/***************/
	// TextureTarget
	/***************/
	class TextureTarget
	{
	public:
		struct PlatformParams
		{
			virtual ~PlatformParams() = 0;

			// Static member functions:
			static void CreatePlatformParams(gr::TextureTarget&);
		};
	};


	/******************/
	// TextureTargetSet
	/******************/
	class TextureTargetSet
	{
	public:
		struct PlatformParams
		{
			virtual ~PlatformParams() = 0;

			// Static member functions:
			static void CreatePlatformParams(gr::TextureTargetSet&);
		};

		// Dynamically-linked static functions:



		static void (*CreateColorTargets)(gr::TextureTargetSet& targetSet, uint32_t firstTextureUnit);
		static void (*AttachColorTargets)(gr::TextureTargetSet const& targetSet, uint32_t face, uint32_t mipLevel, bool doBind);

		static void (*CreateDepthStencilTarget)(gr::TextureTargetSet& targetSet, uint32_t textureUnit);
		static void (*AttachDepthStencilTarget)(gr::TextureTargetSet const& targetSet, bool doBind);

		static uint32_t(*MaxColorTargets)();
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline platform::TextureTarget::PlatformParams::~PlatformParams() {};
	inline platform::TextureTargetSet::PlatformParams::~PlatformParams() {};
}