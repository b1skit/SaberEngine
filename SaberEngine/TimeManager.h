// Time manager class
// Responsible for tracking all time-related info

#pragma once

#include "EngineComponent.h"	// Base class

#include <SDL_timer.h>

namespace SaberEngine
{
	class TimeManager : EngineComponent
	{
	public:
		TimeManager();

		// Singleton functionality:
		static TimeManager& Instance();
		TimeManager(TimeManager const&) = delete; // Disallow copying of our Singleton
		void operator=(TimeManager const&) = delete;
		
		// EngineComponent interface:
		void Startup();
		void Shutdown();
		void Update();
		void Destroy() {}	// Do nothing, for now...

		// Member functions:

		//inline unsigned int GetCurrentTime()
		//{
		//	return m_currentTime;
		//}

		//// Get the time elapsed since the last frame, in seconds
		//static double GetDeltaTimeSeconds()
		//{
		//	return (double)DeltaTime() * 0.001; // Convert: ms->sec
		//}

		// Get the time elapsed since the last frame, in ms
		static inline unsigned int	DeltaTime()						{ return m_deltaTime; }
		static unsigned int			GetTotalRunningTimeMs()			{ return m_currentTime - m_startTime; }
		static double				GetTotalRunningTimeSeconds()	{ return (double)GetTotalRunningTimeMs() * 0.001; }

		
	protected:


	private:
		static unsigned int m_startTime;
		static unsigned int m_prevTime;
		static unsigned int m_currentTime;
		static unsigned int	m_deltaTime;

		/*double timeScale;*/
	};

	
}


