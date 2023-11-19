// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "Context.h"
#include "Context_Platform.h"
#include "Context_OpenGL.h"
#include "Context_DX12.h"

using en::Config;


namespace platform
{
	void (*platform::Context::Destroy)(re::Context& context) = nullptr;
}