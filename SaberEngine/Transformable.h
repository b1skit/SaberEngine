#pragma once

#include "NamedObject.h"
#include "EventListener.h"
#include "Transform.h"


namespace fr
{
	class Transformable
	{
	public:
		Transformable(gr::Transform* parent) : m_transform(parent) {}
		Transformable(const Transformable& rhs) = default;
		Transformable(Transformable&&) = default;
		Transformable& operator=(Transformable const&) = default;

		virtual ~Transformable() = 0;

		// Getters/Setters:
		inline gr::Transform* GetTransform() { return &m_transform; }
		inline gr::Transform const* GetTransform() const { return &m_transform; }


	protected:
		gr::Transform m_transform;

	private:
		Transformable() = delete;
	};

	// We need to provide a destructor implementation since it's pure virtual
	inline Transformable::~Transformable() {}
}
