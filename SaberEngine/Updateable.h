#pragma once

namespace en
{
	// Updateable Interface: For objects that require ticking each frame.
	// Objects using this interface will likely need to be added to a tracking list in SceneData, so Update() can be
	// pumped each frame by the SceneManager
	class Updateable
	{
	public:
		Updateable() = default;
		Updateable(Updateable const&) = default;
		Updateable(Updateable&&) = default;
		Updateable& operator=(Updateable const&) = default;
		~Updateable() = default;
		
		// Updateable interface:
		virtual void Update() = 0;
	};
}
