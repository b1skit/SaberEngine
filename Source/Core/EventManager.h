// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"

#include "Interfaces/IEngineComponent.h"

#include "Util/HashKey.h"


namespace core
{
	class IEventListener;
}
namespace re
{
	class Context;
}

// Common event keys:
namespace eventkey
{
	// Generic events: These will likely have packed data that needs to be interpreted
	constexpr util::HashKey KeyEvent("KeyEvent");
	constexpr util::HashKey MouseMotionEvent("MouseMotionEvent");
	constexpr util::HashKey MouseButtonEvent("MouseButtonEvent");
	constexpr util::HashKey MouseWheelEvent("MouseWheelEvent");
	constexpr util::HashKey TextInputEvent("TextInputEvent");

	constexpr util::HashKey KeyboardInputCaptureChange("KeyboardInputCaptureChange");
	constexpr util::HashKey MouseInputCaptureChange("MouseInputCaptureChange");

	// Functionality triggers: Typically a system will be interested in these, not specific button states
	constexpr util::HashKey InputForward("InputForward");
	constexpr util::HashKey InputBackward("InputBackward");
	constexpr util::HashKey InputLeft("InputLeft");
	constexpr util::HashKey InputRight("InputRight");
	constexpr util::HashKey InputUp("InputUp");
	constexpr util::HashKey InputDown("InputDown");
	constexpr util::HashKey InputSprint("InputSprint");

	// Mouse functions:
	constexpr util::HashKey InputMouseLeft("InputMouseLeft");
	constexpr util::HashKey InputMouseMiddle("InputMouseMiddle");
	constexpr util::HashKey InputMouseRight("InputMouseRight");

	// System:
	constexpr util::HashKey ToggleConsole("ToggleConsole");
	constexpr util::HashKey ToggleVSync("ToggleVSync");
	constexpr util::HashKey VSyncModeChanged("VSyncModeChanged");
	constexpr util::HashKey WindowFocusChanged("WindowFocusChanged");
	constexpr util::HashKey DragAndDrop("DragAndDropEvent");

	constexpr util::HashKey EngineQuit("EngineQuit");

	constexpr util::HashKey FileImportRequest("FileImportRequest");
	constexpr util::HashKey SceneCreated("SceneCreated");
	constexpr util::HashKey SceneResetRequest("SceneResetRequest");
}

namespace core
{
	class EventManager final : public virtual en::IEngineComponent
	{
	public:
		using EventData = std::variant<
			bool, 
			int32_t, 
			uint32_t, 
			float, 
			char, 
			char const*, 
			std::string, 
			std::pair<int32_t, int32_t>,
			std::pair<uint32_t, bool>,
			std::pair<uint32_t, uint32_t>,
			std::pair<float, float>>;

		struct EventInfo
		{
			util::HashKey m_eventKey = util::HashKey("UninitializedEvent");
			EventData m_data;
		};


	public:
		static EventManager* Get(); // Singleton functionality

	public:
		EventManager();

		EventManager(EventManager&&) noexcept = default;
		EventManager& operator=(EventManager&&) noexcept = default;
		~EventManager() = default;
		
		// IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		// Member functions:
		void Subscribe(util::HashKey const& eventType, IEventListener* listener); // Subscribe to an event
		void Notify(EventInfo&&); // Post an event


	private:
		std::vector<EventInfo> m_eventQueue;
		std::mutex m_eventQueueMutex;

		std::unordered_map<util::HashKey, std::vector<IEventListener*>> m_eventListeners;
		std::mutex m_eventListenersMutex;


	private:
		EventManager(EventManager const&) = delete;
		void operator=(EventManager const&) = delete;
	};


}