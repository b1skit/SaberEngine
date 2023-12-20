// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "NamedObject.h"
#include "Transform.h"


namespace fr
{
	// ECS_CONVERSION: DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	class Transformable : public virtual en::NamedObject
	{
	public:
		Transformable(std::string const& name, gr::Transform* parent);

		virtual ~Transformable() = 0;

		inline gr::Transform* GetTransform() { return &m_transform; }
		inline gr::Transform const* GetTransform() const { return &m_transform; }


	protected:
		gr::Transform m_transform;


	private:// The SceneData holds a list of raw Transformable*, no moving/copying allowed
		Transformable(const Transformable& rhs) = delete;
		Transformable(Transformable&&) = delete;
		Transformable& operator=(Transformable const&) = delete;
		Transformable& operator=(Transformable&&) = delete;

		Transformable() = delete;
	};
}
