// © 2022 Adam Badke. All rights reserved.
#include "Context_Platform.h"


namespace platform
{
	void (*platform::Context::Destroy)(re::Context& context) = nullptr;
}