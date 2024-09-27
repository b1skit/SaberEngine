// © 2024 Adam Badke. All rights reserved.
#include "BufferInput.h"
#include "Buffer.h"


namespace re
{
	BufferInput::BufferInput()
		: core::INamedObject("Invalid_DefaultConstructedBufferInput")
		, m_buffer(nullptr)
	{
	}


	BufferInput::BufferInput(std::string const& shaderName, std::shared_ptr<re::Buffer> buffer)
		: core::INamedObject(shaderName)
		, m_buffer(buffer)
	{
	}


	BufferInput::BufferInput(char const* shaderName, std::shared_ptr<re::Buffer> buffer)
		: core::INamedObject(shaderName)
		, m_buffer(buffer)
	{
	}


	void BufferInput::Release()
	{
		m_buffer = nullptr;
	}
}