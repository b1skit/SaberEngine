#pragma once

#include <memory>
#include <string>

#include "ParameterBlock_Platform.h"


namespace re
{
	// Immutable parameter block that is allocated/buffered at instantiation/Create, and deallocated/destroyed when the
	// destructor is called
	class PermanentParameterBlock
	{
	public: 
		template<typename T>
		static std::shared_ptr<re::PermanentParameterBlock> Create(std::string paramBlockName, std::shared_ptr<T> data)
		{
			return make_shared<re::PermanentParameterBlock>(Accessor(), paramBlockName, data, sizeof(T));
		}

	public:
		PermanentParameterBlock() = delete;

		~PermanentParameterBlock() { Destroy(); };
		PermanentParameterBlock(PermanentParameterBlock const&) = delete;
		PermanentParameterBlock(PermanentParameterBlock&&) = delete;
		PermanentParameterBlock& operator=(PermanentParameterBlock const&) = delete;

	public:

		inline std::string const& Name() const { return m_paramBlockName; }		

		inline void const* const GetData() const { return m_data.get(); }
		size_t GetDataSize() const { return m_dataSizeInBytes; }

		inline platform::PermanentParameterBlock::PlatformParams* const GetPlatformParams() const { return m_platformParams.get(); }

	private:		
		std::string m_paramBlockName; // Shader name

		std::shared_ptr<void> m_data;
		size_t m_dataSizeInBytes;

		std::unique_ptr<platform::PermanentParameterBlock::PlatformParams> m_platformParams;
		
		void Destroy();
		

	private:
		struct Accessor { explicit Accessor() = default; }; // Prevents access to the CTOR
	public:
		template <typename T>
		PermanentParameterBlock(
			PermanentParameterBlock::Accessor, std::string paramBlockName, std::shared_ptr<T> data,size_t dataSizeInBytes) :
			m_paramBlockName(paramBlockName),
			m_data(data),
			m_dataSizeInBytes(dataSizeInBytes)
		{
			platform::PermanentParameterBlock::PlatformParams::CreatePlatformParams(*this);
			platform::PermanentParameterBlock::Create(*this);
		}

	private:
		// Friends:
		friend void platform::PermanentParameterBlock::PlatformParams::CreatePlatformParams(re::PermanentParameterBlock&);

		template<typename T>
		friend std::shared_ptr<re::PermanentParameterBlock> PermanentParameterBlock::Create(std::string, std::shared_ptr<T>);
	};
}