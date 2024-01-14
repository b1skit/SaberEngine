// © 2024 Adam Badke. All rights reserved.
#include "Assert.h"
#include "CoreEngine.h"
#include "Window.h"


#if defined(_DEBUG)

void HandleAssertInternal()
{
	en::CoreEngine::Get()->GetWindow()->SetRelativeMouseMode(false);
}

#endif