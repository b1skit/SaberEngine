// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "AccelerationStructure_Platform.h"
#include "RenderManager.h"


namespace re
{
	std::shared_ptr<AccelerationStructure> AccelerationStructure::CreateBLAS(
		char const* name,
		std::unique_ptr<ICreateParams>&& blasCreateParams)
	{
		SEAssert(blasCreateParams && dynamic_cast<BLASCreateParams*>(blasCreateParams.get()),
			"Invalid blas create params");

		std::shared_ptr<AccelerationStructure> newAccelerationStructure;
		newAccelerationStructure.reset(
			new AccelerationStructure(
				name, 
				AccelerationStructure::Type::BLAS, 
				std::move(blasCreateParams)));

		re::RenderManager::Get()->RegisterForCreate(newAccelerationStructure);

		return newAccelerationStructure;
	}


	AccelerationStructure::~AccelerationStructure()
	{
		Destroy();
	}


	void AccelerationStructure::Create()
	{
		platform::AccelerationStructure::Create(*this);
	}


	void AccelerationStructure::Destroy()
	{
		if (m_platformParams)
		{
			re::RenderManager::Get()->RegisterForDeferredDelete(std::move(m_platformParams));
		}
	}


	AccelerationStructure::AccelerationStructure(char const* name, Type type, std::unique_ptr<ICreateParams>&& createParams)
		: core::INamedObject(name)
		, m_platformParams(platform::AccelerationStructure::CreatePlatformParams())
		, m_createParams(std::move(createParams))
		, m_type(type)
	{
	}
}