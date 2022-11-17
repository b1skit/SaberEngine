#pragma once

#include "EngineComponent.h"


namespace en
{
	class TimeManager : public virtual en::EngineComponent
	{
	public:
		TimeManager();
		
		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		void Destroy() { /*Do nothing*/ }

		// Milliseconds (ms) elapsed since last frame
		static inline uint64_t	DeltaTimeMs() { return m_deltaTimeMs; }


	private:
		static uint64_t m_prevTimeMs;
		static uint64_t m_currentTimeMs;
		static uint64_t	m_deltaTimeMs;

	private:
		TimeManager(TimeManager const&) = delete;
		TimeManager(TimeManager&&) = delete;
		void operator=(TimeManager const&) = delete;
	};

	
}


