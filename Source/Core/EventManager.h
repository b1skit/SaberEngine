// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"

#include "Interfaces/IEngineComponent.h"

#include "Util/CHashKey.h"


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
	constexpr util::CHashKey ToggleConsole("ToggleConsole");
	constexpr util::CHashKey ToggleVSync("ToggleVSync");
	constexpr util::CHashKey VSyncModeChanged("VSyncModeChanged");
	constexpr util::CHashKey WindowFocusChanged("WindowFocusChanged");
	constexpr util::CHashKey DragAndDrop("DragAndDropEvent");

	constexpr util::CHashKey EngineQuit("EngineQuit");

	constexpr util::CHashKey FileImportRequest("FileImportRequest");
	constexpr util::CHashKey SceneCreated("SceneCreated");
	constexpr util::CHashKey SceneResetRequest("SceneResetRequest");
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
			util::CHashKey m_eventKey = util::CHashKey("UninitializedEvent");
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
		void Subscribe(util::CHashKey const& eventType, IEventListener* listener); // Subscribe to an event
		void Notify(EventInfo&&); // Post an event


	private:
		std::vector<EventInfo> m_eventQueue;
		std::mutex m_eventQueueMutex;

		std::unordered_map<util::CHashKey, std::vector<IEventListener*>> m_eventListeners;
		std::mutex m_eventListenersMutex;


	private:
		EventManager(EventManager const&) = delete;
		void operator=(EventManager const&) = delete;
	};


}