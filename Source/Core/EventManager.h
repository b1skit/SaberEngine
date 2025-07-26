// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Util/CHashKey.h"


namespace core
{
	class IEventListener;
}
namespace re
{
	class Context;
}

namespace core
{
	class EventManager final
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
		// IEngineComponent interface:
		static void Startup();
		static void Shutdown();
		static void Update();

		// Member functions:
		static void Subscribe(util::CHashKey const& eventType, IEventListener* listener); // Subscribe to an event
		static void Notify(EventInfo&&); // Post an event


	private:
		static std::vector<EventInfo> s_eventQueue;
		static std::mutex s_eventQueueMutex;

		static std::unordered_map<util::CHashKey, std::vector<IEventListener*>> s_eventListeners;
		static std::mutex s_eventListenersMutex;


	private: // Pure static only:
		EventManager() = delete;
		EventManager(EventManager const&) = delete;
		EventManager(EventManager&&) noexcept = delete;
		EventManager& operator=(EventManager&&) noexcept = delete;		
		void operator=(EventManager const&) = delete;
		~EventManager() = default;
	};


}