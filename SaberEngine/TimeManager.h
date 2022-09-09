#pragma once

#include "EngineComponent.h"


namespace en
{
	class TimeManager : public virtual en::EngineComponent
	{
	public:
		TimeManager();

		TimeManager(TimeManager const&) = delete;
		TimeManager(TimeManager&&) = delete;
		void operator=(TimeManager const&) = delete;
		
		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		void Destroy() { /*Do nothing*/ }

		// Member functions:		
		static inline unsigned int	DeltaTime()						{ return m_deltaTime; } // ms elapsed since last frame
		static unsigned int			GetTotalRunningTimeMs()			{ return m_currentTime - m_startTime; }
		static double				GetTotalRunningTimeSeconds()	{ return (double)GetTotalRunningTimeMs() * 0.001; }

	private:
		static unsigned int m_startTime;
		static unsigned int m_prevTime;
		static unsigned int m_currentTime;
		static unsigned int	m_deltaTime;
	};

	
}


