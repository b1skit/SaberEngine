// © 2024 Adam Badke. All rights reserved.
#include "Assert.h"
#include "EngineApp.h"
#include "Window.h"


#if defined(_DEBUG)

void HandleAssertInternal()
{
	en::EngineApp::Get()->GetWindow()->SetRelativeMouseMode(false);
}

#endif