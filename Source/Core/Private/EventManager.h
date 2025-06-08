// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/Assert.h"

#include "Private/Interfaces/IEngineComponent.h"

#include "Private/Util/CHashKey.h"


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