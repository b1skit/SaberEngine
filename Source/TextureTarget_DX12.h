// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "TextureTarget.h"
#include "TextureTarget_Platform.h"


namespace dx12
{
class TextureTarget
{
public:
	struct PlatformParams final : public virtual re::TextureTarget::PlatformParams
	{

	};

};


class TextureTargetSet
{
public:
	struct PlatformParams final : public virtual re::TextureTargetSet::PlatformParams
	{

	};

	// Static members:
	static void CreateColorTargets(re::TextureTargetSet& targetSet);
	static void AttachColorTargets(re::TextureTargetSet& targetSet, uint32_t face, uint32_t mipLevel);

	static void CreateDepthStencilTarget(re::TextureTargetSet& targetSet);
	static void AttachDepthStencilTarget(re::TextureTargetSet& targetSet);
};
}