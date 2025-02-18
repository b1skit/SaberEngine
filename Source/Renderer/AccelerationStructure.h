// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "VertexStream.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IPlatformParams.h"
#include "Core/Interfaces/INamedObject.h"


namespace re
{
	class AccelerationStructure : public virtual core::INamedObject
	{
	public:
		enum class Type : bool
		{
			TLAS,
			BLAS,
		};

		enum GeometryFlags : uint8_t
		{
			None						= 0,
			Opaque						= 1 << 0,
			NoDuplicateAnyHitInvocation = 1 << 1, // Guarantee the any hit shader will be executed exactly once
		};

		enum BuildFlags : uint8_t
		{
			Default			= 0, // None
			AllowUpdate		= 1 << 0,
			AllowCompaction = 1 << 1,
			PreferFastTrace = 1 << 2,
			PreferFastBuild = 1 << 3,
			MinimizeMemory	= 1 << 4,
			PerformUpdate	= 1 << 5,
		};


		struct ICreateParams
		{
			virtual ~ICreateParams() = 0;
		};

		struct BLASCreateParams : public virtual ICreateParams
		{
			struct Instance
			{
				core::InvPtr<gr::VertexStream> m_positions;
				core::InvPtr<gr::VertexStream> m_indices; // Can be null/invalid

				GeometryFlags m_geometryFlags;
			};
			std::vector<Instance> m_instances;
			std::shared_ptr<re::Buffer> m_transform; // Buffer of mat3x4 in row-major order. Indexes correspond with instances

			BuildFlags m_buildFlags;
		};


	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual void Destroy() override = 0;
		};


	public:
		static std::shared_ptr<AccelerationStructure> CreateBLAS(
			char const* name,
			std::unique_ptr<ICreateParams>&& blasCreateParams);


	public:
		AccelerationStructure() = default;
		AccelerationStructure(AccelerationStructure&&) noexcept = default;
		AccelerationStructure& operator=(AccelerationStructure&&) noexcept = default;

		~AccelerationStructure();

		void Create();
		void Destroy();

		PlatformParams* GetPlatformParams() const;


	public:

		ICreateParams const* GetCreateParams() const;
		void ReleaseCreateParams();

		Type GetType() const;


	private:
		AccelerationStructure(char const* name, Type, std::unique_ptr<ICreateParams>&&); // Use Create() instead


	private:
		std::unique_ptr<PlatformParams> m_platformParams;

		std::unique_ptr<ICreateParams> m_createParams;

		Type m_type;


	private: // No copies allowed
		AccelerationStructure(AccelerationStructure const&) = delete;
		AccelerationStructure& operator=(AccelerationStructure const&) = delete;
	};


	inline re::AccelerationStructure::ICreateParams::~ICreateParams() {} // Pure virtual: Must provide an impl


	inline AccelerationStructure::PlatformParams* AccelerationStructure::GetPlatformParams() const
	{
		return m_platformParams.get();
	}


	inline AccelerationStructure::ICreateParams const* AccelerationStructure::GetCreateParams() const
	{
		return m_createParams.get();
	}


	inline void AccelerationStructure::ReleaseCreateParams()
	{
		m_createParams = nullptr;
	}


	inline AccelerationStructure::Type AccelerationStructure::GetType() const
	{
		return m_type;
	}
}