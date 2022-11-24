#pragma once

#include <memory>


namespace gr
{
	class Texture;
}
namespace re
{
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
			static void CreatePlatformParams(re::TextureTarget&);
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
			static void CreatePlatformParams(re::TextureTargetSet&);
		};

		// Dynamically-linked static functions:



		static void (*CreateColorTargets)(re::TextureTargetSet& targetSet);
		static void (*AttachColorTargets)(re::TextureTargetSet const& targetSet, uint32_t face, uint32_t mipLevel, bool doBind);

		static void (*CreateDepthStencilTarget)(re::TextureTargetSet& targetSet);
		static void (*AttachDepthStencilTarget)(re::TextureTargetSet const& targetSet, bool doBind);

		static uint32_t(*MaxColorTargets)();
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline platform::TextureTarget::PlatformParams::~PlatformParams() {};
	inline platform::TextureTargetSet::PlatformParams::~PlatformParams() {};
}