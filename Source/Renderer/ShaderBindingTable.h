// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Effect.h"
#include "Shader.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformObject.h"

#include "_generated/DrawStyles.h"


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
		struct SBTParams final
		{
			std::vector<std::pair<EffectID, effect::drawstyle::Bitmask>> m_rayGenStyles;
			std::vector<std::pair<EffectID, effect::drawstyle::Bitmask>> m_missStyles;
			std::vector<std::pair<EffectID, effect::drawstyle::Bitmask>> m_callableStyles;

			effect::drawstyle::Bitmask m_hitgroupStyles; // Combined with BLAS geo bitmasks to resolve hitgroup Techniques

			uint32_t m_maxPayloadByteSize = 0;
			uint32_t m_maxRecursionDepth = 0;

			bool m_useLocalRootSignatures = false;
		};
		static std::shared_ptr<re::ShaderBindingTable> Create(
			char const* name, SBTParams const&, std::shared_ptr<re::AccelerationStructure> const&);

		ShaderBindingTable(ShaderBindingTable&&) noexcept = default;
		ShaderBindingTable& operator=(ShaderBindingTable&&) noexcept = default;

		~ShaderBindingTable();


	public:
		void Destroy();

		PlatObj* GetPlatformObject() const;

		SBTParams const& GetSBTParams() const;


	private:
		void Initialize(std::shared_ptr<re::AccelerationStructure> const&);


	private:
		std::unique_ptr<PlatObj> m_platObj;


	private:
		friend class dx12::ShaderBindingTable;
		std::vector<core::InvPtr<re::Shader>> m_rayGenShaders;
		std::vector<core::InvPtr<re::Shader>> m_missShaders;
		std::vector<std::pair<std::string, core::InvPtr<re::Shader>>> m_hitGroupNamesAndShaders; // Order matches BLAS instances
		std::vector<core::InvPtr<re::Shader>> m_callableShaders;
		
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