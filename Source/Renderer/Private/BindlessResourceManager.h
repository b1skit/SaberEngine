// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "EnumTypes.h"

#include "Core/Interfaces/IPlatformObject.h"


using ResourceHandle = uint32_t;	// Array index into overlapped unbounded descriptor arrays

namespace re
{
	struct IBindlessResource
	{
		virtual ~IBindlessResource() = default;

		virtual void GetPlatformResource(void* resourceOut, size_t resourceOutByteSize) const = 0;
		virtual void GetDescriptor(void* descriptorOut, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const = 0;

		// Optional: Returns a default state otherwise
		virtual void GetResourceUseState(void* dest, size_t destByteSize) const;

		re::ViewType m_viewType = re::ViewType::SRV;
	};

	
	// ---


	class BindlessResourceManager
	{
	public:
		static constexpr uint32_t k_initialResourceCount = 32;
		static constexpr float k_growthFactor = 1.5f;


	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual void Destroy() override = 0;

			std::mutex m_platformParamsMutex;

			// Total number (both in use and available) of resource indexes
			uint32_t m_currentMaxIndex = BindlessResourceManager::k_initialResourceCount;

			bool m_isCreated = false;
		};


	public:
		BindlessResourceManager();

		BindlessResourceManager(BindlessResourceManager&&) noexcept = default;
		BindlessResourceManager& operator=(BindlessResourceManager&&) noexcept = default;
		~BindlessResourceManager() = default;

		void Destroy();


	public:
		void Update(uint64_t frameNum);


	public:
		ResourceHandle RegisterResource(std::unique_ptr<IBindlessResource>&&);
		void UnregisterResource(ResourceHandle&, uint64_t frameNum);


	public:
		PlatObj* GetPlatformObject() const;


	private:
		void Initialize(uint64_t frameNum);
		void IncreaseSetSize();

		void ProcessRegistrations();
		void ProcessUnregistrations(uint64_t frameNum);


	private:
		std::mutex m_brmMutex;

		struct UnregistrationMetadata final
		{
			uint64_t m_unregistrationFrameNum; // Frame number the Resource was unregistered on
			ResourceHandle m_resourceHandle;
		};
		std::queue<UnregistrationMetadata> m_unregistrations;

		// We use a priority queue to ensure that ResourceHandles closest to 0 are reused first, to minimize the number
		// of descriptors that are copied each frame
		std::priority_queue<ResourceHandle, std::vector<ResourceHandle>, std::greater<ResourceHandle>> m_freeIndexes;

		struct RegistrationMetadata final
		{
			std::unique_ptr<IBindlessResource> m_resource;
			ResourceHandle m_resourceHandle;
		};
		std::vector<RegistrationMetadata> m_registrations;

		std::unique_ptr<PlatObj> m_platObj;

		bool m_mustReinitialize;
		uint8_t m_numFramesInFlight;


	private: // No copies allowed:
		BindlessResourceManager(BindlessResourceManager const&) = delete;
		BindlessResourceManager& operator=(BindlessResourceManager const&) = delete;
	};


	inline BindlessResourceManager::PlatObj* BindlessResourceManager::GetPlatformObject() const
	{
		return m_platObj.get();
	}
}