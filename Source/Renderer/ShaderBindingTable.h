// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Effect.h"
#include "Shader.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformObject.h"

#include "Generated/DrawStyles.h"


namespace dx12
{
	class ShaderBindingTable;
}
namespace re
{
	class AccelerationStructure;


	class ShaderBindingTable : public virtual core::INamedObject
	{
	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual void Destroy() override = 0;
		};


	public:
		struct SBTParams
		{
			std::vector<std::pair<EffectID, effect::drawstyle::Bitmask>> m_rayGenStyles;
			std::vector<std::pair<EffectID, effect::drawstyle::Bitmask>> m_missStyles;
			std::vector<std::pair<EffectID, effect::drawstyle::Bitmask>> m_callableStyles;

			effect::drawstyle::Bitmask m_hitgroupStyles; // Combined with BLAS geo bitmasks to resolve hitgroup Techniques

			uint32_t m_maxPayloadByteSize = 0;
			uint32_t m_maxRecursionDepth = 0;

			bool m_useLocalRootSignatures = false;
		};
		static std::shared_ptr<re::ShaderBindingTable> Create(char const* name, SBTParams const&);

		ShaderBindingTable(ShaderBindingTable&&) noexcept = default;
		ShaderBindingTable& operator=(ShaderBindingTable&&) noexcept = default;

		~ShaderBindingTable();


	public:
		void Destroy();

		PlatObj* GetPlatformObject() const;

		SBTParams const& GetSBTParams() const;


	public:
		// This should be called every frame (in case the TLAS has been re/created/modified etc)
		// Note: Replaces the std::shared_ptr<re::ShaderBindingTable> if the underlying object is recreated
		static void Update(std::shared_ptr<re::ShaderBindingTable>&, std::shared_ptr<re::AccelerationStructure> const&);


	private:
		std::unique_ptr<PlatObj> m_platObj;


	private:
		friend class dx12::ShaderBindingTable;
		std::vector<core::InvPtr<re::Shader>> m_rayGenShaders;
		std::vector<core::InvPtr<re::Shader>> m_missShaders;
		std::vector<std::pair<std::string, core::InvPtr<re::Shader>>> m_hitGroupNamesAndShaders; // Order matches BLAS instances
		std::vector<core::InvPtr<re::Shader>> m_callableShaders;

		std::shared_ptr<re::AccelerationStructure> m_TLAS;

		SBTParams m_sbtParams;


	private:
		ShaderBindingTable(char const* name, SBTParams const&); // Use Create()


	private: // No copies allowed
		ShaderBindingTable() = delete;
		ShaderBindingTable(ShaderBindingTable const&) = delete;
		ShaderBindingTable& operator=(ShaderBindingTable const&) = delete;
	};


	inline ShaderBindingTable::PlatObj* ShaderBindingTable::GetPlatformObject() const
	{
		return m_platObj.get();
	}


	inline ShaderBindingTable::SBTParams const& ShaderBindingTable::GetSBTParams() const
	{
		return m_sbtParams;
	}
}