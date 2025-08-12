// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "../Util/CHashKey.h"


namespace eventkey
{
	// Generic events: These will likely have packed data that needs to be interpreted
	constexpr util::CHashKey KeyEvent("KeyEvent");
	constexpr util::CHashKey MouseMotionEvent("MouseMotionEvent");
	constexpr util::CHashKey MouseButtonEvent("MouseButtonEvent");
	constexpr util::CHashKey MouseWheelEvent("MouseWheelEvent");
	constexpr util::CHashKey TextInputEvent("TextInputEvent");

	constexpr util::CHashKey KeyboardInputCaptureChange("KeyboardInputCaptureChange");
	constexpr util::CHashKey MouseInputCaptureChange("MouseInputCaptureChange");

	// Functionality triggers: Typically a system will be interested in these, not specific button states
	constexpr util::CHashKey InputForward("InputForward");
	constexpr util::CHashKey InputBackward("InputBackward");
	constexpr util::CHashKey InputLeft("InputLeft");
	constexpr util::CHashKey InputRight("InputRight");
	constexpr util::CHashKey InputUp("InputUp");
	constexpr util::CHashKey InputDown("InputDown");
	constexpr util::CHashKey InputSprint("InputSprint");

	// Mouse functions:
	constexpr util::CHashKey InputMouseLeft("InputMouseLeft");
	constexpr util::CHashKey InputMouseMiddle("InputMouseMiddle");
	constexpr util::CHashKey InputMouseRight("InputMouseRight");

	// System:
	constexpr util::CHashKey ToggleFreeLook("ToggleFreeLook");
	constexpr util::CHashKey TogglePerformanceTimers("TogglePerformanceTimers");
	constexpr util::CHashKey ToggleVSync("ToggleVSync");
	constexpr util::CHashKey VSyncModeChanged("VSyncModeChanged");
	constexpr util::CHashKey ToggleUIVisibility("ToggleUIVisibility");
	constexpr util::CHashKey WindowFocusChanged("WindowFocusChanged");
	constexpr util::CHashKey DragAndDrop("DragAndDropEvent");

	constexpr util::CHashKey EngineQuit("EngineQuit");

	constexpr util::CHashKey FileImportRequest("FileImportRequest");
	constexpr util::CHashKey SceneCreated("SceneCreated");
	constexpr util::CHashKey SceneResetRequest("SceneResetRequest");
}