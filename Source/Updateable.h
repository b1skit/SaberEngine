// © 2022 Adam Badke. All rights reserved.
#pragma once

namespace en
{
	// Updateable Interface: For objects that require ticking each frame.
	// Objects using this interface will likely need to be added to a tracking list in SceneData, so Update() can be
	// pumped each frame by the SceneManager
	class Updateable
	{
	public:
		Updateable();
		~Updateable();

		// Updateable interface:
		virtual void Update(const double stepTimeMs) = 0;


	private:
		void Register();
		void Unregister() const;


	private: // Updateables self-register raw ptrs of themselves to management systems, no copying/moving allowed
		Updateable(Updateable const&) = delete;
		Updateable(Updateable&&) = delete;
		Updateable& operator=(Updateable const&) = delete;
	};
}
