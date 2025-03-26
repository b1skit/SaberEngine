// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IPlatformParams.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/ThreadProtector.h"


using ResourceHandle = uint32_t;	// Array index into typed IBindlessResourceSet
static constexpr ResourceHandle k_invalidResourceHandle = std::numeric_limits<ResourceHandle>::max();

namespace re
{
	class BindlessResourceManager;
	class IBindlessResourceSet;


	struct IBindlessResource
	{
		virtual ~IBindlessResource() = default;

		virtual void GetPlatformResource(void* resourceOut, size_t resourceOutByteSize) = 0;
		virtual void GetDescriptor(IBindlessResourceSet*, void* descriptorOut, size_t descriptorOutByteSize) = 0;
	};


	// ---

	
	class IBindlessResourceSet
	{
	public:
		static constexpr uint32_t k_initialResourceCount = 1;
		static constexpr float k_growthFactor = 1.5f;


	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual void Destroy() override = 0;

			bool m_isCreated = false;
		};


	public:
		IBindlessResourceSet(BindlessResourceManager*, char const* shaderName);

		virtual ~IBindlessResourceSet() = default;

		void Destroy();

		PlatformParams* GetPlatformParams() const;


	private: // BindlessResourceManager interface:
		friend class BindlessResourceManager;

		ResourceHandle RegisterResource(std::unique_ptr<IBindlessResource>&&);
		void UnregisterResource(ResourceHandle&, uint64_t frameNum);

		void Update(uint64_t frameNum);


	
	private:
		// Store resources pointers and immediately write descriptors to both CPU and GPU-visible heaps. Writes nulls
		// if IBindlessResource* is null
		void SetResource(IBindlessResource*, ResourceHandle);

		
	public: // Platform implementations:
		virtual void GetNullDescriptor(void* dest, size_t destByteSize) const = 0;
		virtual void GetResourceUsageState(void* dest, size_t destByteSize) const = 0;


	public: // Member accessors for platform:
		BindlessResourceManager* GetBindlessResourceManager() const;
		std::string const& GetShaderName() const;
		uint32_t GetCurrentResourceCount() const;
		

	private:
		void ProcessRegistrations();
		void ProcessUnregistrations(uint64_t frameNum);


	private: 
		void IncreaseSetSize();


	private:
		// We use a priority queue to ensure that ResourceHandles closest to 0 are reused first, to minimize the number
		// of descriptors that are copied each frame
		std::priority_queue<ResourceHandle, std::vector<ResourceHandle>, std::greater<ResourceHandle>> m_freeIndexes;
	
		BindlessResourceManager* m_bindlessResourceMgr;

		std::unique_ptr<PlatformParams> m_platformParams;

		std::string m_shaderName; 	// To query root sig metadata (E.g. for setting null descriptors from the BRM)

		uint32_t m_currentResourceCount;	// Max. no. of resources managed by this resource system


	private:
		struct UnregistrationMetadata
		{
			uint64_t m_unregistrationFrameNum; // Frame number the Resource was unregistered on
			ResourceHandle m_resourceHandle;
		};
		std::queue<UnregistrationMetadata> m_unregistrations;

		struct RegistrationMetadata
		{
			std::unique_ptr<IBindlessResource> m_resource;
			ResourceHandle m_resourceHandle;
		};
		std::vector<RegistrationMetadata> m_registrations;


	private:
		mutable util::ThreadProtector m_threadProtector;


	private:
		uint8_t m_numFramesInFlight;
	};


	inline IBindlessResourceSet::PlatformParams* IBindlessResourceSet::GetPlatformParams() const
	{
		return m_platformParams.get();
	}


	inline BindlessResourceManager* IBindlessResourceSet::GetBindlessResourceManager() const
	{
		return m_bindlessResourceMgr;
	}


	inline std::string const& IBindlessResourceSet::GetShaderName() const
	{
		return m_shaderName;
	}


	inline uint32_t IBindlessResourceSet::GetCurrentResourceCount() const
	{
		return m_currentResourceCount;
	}

	
	// ---


	class BindlessResourceManager
	{
	public:
		BindlessResourceManager();

		BindlessResourceManager(BindlessResourceManager&&) noexcept = default;
		BindlessResourceManager& operator=(BindlessResourceManager&&) noexcept = default;
		~BindlessResourceManager() = default;


	public:
		void Update(uint64_t frameNum);

		void Destroy();


	public:
		std::vector<std::unique_ptr<IBindlessResourceSet>> const& GetResourceSets() const;
		uint8_t GetNumResourceSets() const;


	public:
		template<typename T>
		ResourceHandle RegisterResource(std::unique_ptr<IBindlessResource>&&);

		template<typename T>
		void UnregisterResource(ResourceHandle&, uint64_t frameNum);


	private:
		void Initialize();


	private: // ResourceSet management:
		template<typename T>
		IBindlessResourceSet* GetCreateResourceSet();


	private:
		std::vector<std::unique_ptr<IBindlessResourceSet>> m_resourceSets; // Ordered: Index * no. elements = base offset
		std::map<std::type_index, uint8_t> m_resourceSetTypeIdx; // T -> m_resourceSets index

		bool m_mustRecreate;

		uint8_t m_numFramesInFlight;

		mutable util::ThreadProtector m_threadProtector;


	private: // No copies allowed:
		BindlessResourceManager(BindlessResourceManager const&) = delete;
		BindlessResourceManager& operator=(BindlessResourceManager const&) = delete;
	};


	inline std::vector<std::unique_ptr<IBindlessResourceSet>> const& BindlessResourceManager::GetResourceSets() const
	{
		return m_resourceSets;
	}


	inline uint8_t BindlessResourceManager::GetNumResourceSets() const
	{
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		return util::CheckedCast<uint8_t>(m_resourceSets.size());
	}


	template<typename T>
	ResourceHandle BindlessResourceManager::RegisterResource(std::unique_ptr<IBindlessResource>&& bindlessResource)
	{
		IBindlessResourceSet* resourceSet = GetCreateResourceSet<T>();

		util::ScopedThreadProtector threadProtector(m_threadProtector);
		
		return resourceSet->RegisterResource(std::move(bindlessResource));
	}


	template<typename T>
	void BindlessResourceManager::UnregisterResource(ResourceHandle& resourceIdx, uint64_t frameNum)
	{
		IBindlessResourceSet* resourceSet = GetCreateResourceSet<T>();

		util::ScopedThreadProtector threadProtector(m_threadProtector);

		// Notify the resource set of the unregistration:
		resourceSet->UnregisterResource(resourceIdx, frameNum);
	}


	template<typename T>
	IBindlessResourceSet* BindlessResourceManager::GetCreateResourceSet()
	{
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		const std::type_index typeIdx = std::type_index(typeid(T));

		IBindlessResourceSet* resourceSet = nullptr;

		auto resourceSetIdxItr = m_resourceSetTypeIdx.find(typeIdx);
		if (resourceSetIdxItr == m_resourceSetTypeIdx.end())
		{
			const uint8_t resourceSetIdx = util::CheckedCast<uint8_t>(m_resourceSets.size());

	
			m_resourceSets.emplace_back(T::CreateBindlessResourceSet(this));
			resourceSet = m_resourceSets.back().get();

			m_resourceSetTypeIdx.emplace(typeIdx, resourceSetIdx);

			m_mustRecreate = true;
		}
		else
		{
			resourceSet = m_resourceSets[resourceSetIdxItr->second].get();
		}

		return resourceSet;
	}
}