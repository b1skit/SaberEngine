// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"


namespace re
{
	class Buffer;


	class BufferInput : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		BufferInput();

		BufferInput(std::string const& shaderName, std::shared_ptr<re::Buffer>);
		BufferInput(char const* shaderName, std::shared_ptr<re::Buffer>);

		BufferInput(BufferInput const&) = default;
		BufferInput(BufferInput&&) noexcept = default;

		BufferInput& operator=(BufferInput const&) = default;
		BufferInput& operator=(BufferInput&&) noexcept = default;

		~BufferInput() = default;


	public:
		re::Buffer const* GetBuffer() const;
		re::Buffer* GetBuffer();
		
		std::shared_ptr<re::Buffer> const& GetBufferSharedPtr() const;

		std::string const& GetShaderName() const;
		util::StringHash GetShaderNameHash() const;

		bool IsValid() const;
		void Release();


	private:
		std::shared_ptr<re::Buffer> m_buffer;
	};


	inline re::Buffer const* BufferInput::GetBuffer() const
	{
		return m_buffer.get();
	}


	inline re::Buffer* BufferInput::GetBuffer()
	{
		return m_buffer.get();
	}


	inline std::shared_ptr<re::Buffer> const& BufferInput::GetBufferSharedPtr() const
	{
		return m_buffer;
	}


	inline std::string const& BufferInput::GetShaderName() const
	{
		return GetName();
	}


	inline util::StringHash BufferInput::GetShaderNameHash() const
	{
		return GetNameHash();
	}


	inline bool BufferInput::IsValid() const
	{
		return m_buffer != nullptr;
	}
}