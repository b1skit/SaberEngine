// © 2025 Adam Badke. All rights reserved.
#include "ILoadContext.h"


namespace core
{
	re::Context* ILoadContextBase::s_context = nullptr;
	pr::EntityManager* ILoadContextBase::s_entityManager = nullptr;
}