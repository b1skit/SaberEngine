// © 2024 Adam Badke. All rights reserved.
#include "BufferView.h"
#include "Buffer.h"

#include "Core/Util/HashUtils.h"


namespace re
{
	BufferView::BufferView(BufferType const& view)
		: m_buffer(view)
		, m_dataHash(0)
	{
		util::AddDataBytesToHash(m_dataHash, m_buffer);
	}


	BufferView::BufferView(std::shared_ptr<re::Buffer> const& buffer)
		: m_dataHash(0)
	{
		const uint32_t bufferArraySize = buffer->GetArraySize();

		m_buffer = BufferView::BufferType{
			.m_firstElement = 0, 
			.m_numElements = bufferArraySize,
			.m_structuredByteStride = buffer->GetTotalBytes() / bufferArraySize,
			.m_firstDestIdx = 0,
		};

		util::AddDataBytesToHash(m_dataHash, m_buffer);
	}


	BufferView::BufferView(VertexStreamType const& view)
		: m_stream(view)
		, m_dataHash(0)
	{
		util::AddDataBytesToHash(m_dataHash, m_stream);
	}


	BufferView::BufferView()
		: m_buffer{ BufferView::BufferType{} }
		, m_dataHash(0)
	{
		/* Don't use this directly */
	}


	// -----------------------------------------------------------------------------------------------------------------


	BufferInput::BufferInput()
		: core::INamedObject("Invalid_DefaultConstructedBufferInput")
		, m_buffer(nullptr)
	{
	}


	BufferInput::BufferInput(
		std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
		: core::INamedObject(shaderName)
		, m_buffer(buffer)
		, m_view(view)
	{
	}


	BufferInput::BufferInput(char const* shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
		: core::INamedObject(shaderName)
		, m_buffer(buffer)
		, m_view(view)
	{
	}


	BufferInput::BufferInput(char const* shaderName, std::shared_ptr<re::Buffer> const& buffer)
		: core::INamedObject(shaderName)
		, m_buffer(buffer)
	{
		m_view = re::BufferView(buffer);
	}


	BufferInput::BufferInput(std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer)
		:BufferInput(shaderName.c_str(), buffer)
	{
	}


	void BufferInput::Release()
	{
		m_buffer = nullptr;
	}
}