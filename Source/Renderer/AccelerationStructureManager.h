// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IPlatformParams.h"


namespace re
{
	class AccelerationStructureManager
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			//
		};


	public:
		AccelerationStructureManager() = default;
		AccelerationStructureManager(AccelerationStructureManager&&) = default;
		AccelerationStructureManager& operator=(AccelerationStructureManager&&) = default;
		~AccelerationStructureManager() = default;
		
	public:
		void Create();
		void Update();
		void Destroy();

		void SetPlatformParams(std::unique_ptr<PlatformParams>&&);


	private:
		std::unique_ptr<PlatformParams> m_platformParams;


	private: // No copies allowed
		AccelerationStructureManager(AccelerationStructureManager const&) = delete;
		AccelerationStructureManager& operator=(AccelerationStructureManager const&) = delete;
	};


	inline void AccelerationStructureManager::SetPlatformParams(std::unique_ptr<PlatformParams>&& params)
	{
		m_platformParams = std::move(params);
	}
}