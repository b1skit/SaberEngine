// © 2024 Adam Badke. All rights reserved.
#include "BufferView.h"
#include "Buffer.h"

#include "Core/Assert.h"

#include "Core/Util/HashUtils.h"


namespace re
{
	BufferView::BufferView(BufferType&& view) noexcept
		: m_bufferView(std::move(view))
		, m_isVertexStreamView(false)
	{
		util::AddDataBytesToHash(m_dataHash, m_bufferView);
		util::AddDataBytesToHash(m_dataHash, m_isVertexStreamView);
	}


	BufferView::BufferView(BufferType const& view)
		: BufferView(BufferType(view))
	{
	}


	BufferView::BufferView(std::shared_ptr<re::Buffer> const& buffer)
		: m_isVertexStreamView(false)
	{
		SEAssert(buffer, "Buffer is null");

		const uint32_t bufferArraySize = buffer->GetArraySize();

		m_bufferView = BufferView::BufferType{
			.m_firstElement = 0, 
			.m_numElements = bufferArraySize,
			.m_structuredByteStride = buffer->GetTotalBytes() / bufferArraySize,
			.m_firstDestIdx = 0,
		};

		util::AddDataBytesToHash(m_dataHash, m_bufferView);
		util::AddDataBytesToHash(m_dataHash, m_isVertexStreamView);
	}


	BufferView::BufferView(VertexStreamType&& view) noexcept
		: m_streamView(std::move(view))
		, m_isVertexStreamView(true)
	{
		util::AddDataBytesToHash(m_dataHash, m_streamView);
		util::AddDataBytesToHash(m_dataHash, m_isVertexStreamView);
	}


	BufferView::BufferView(VertexStreamType const& view)
		: BufferView(VertexStreamType(view))
	{
	}


	BufferView::BufferView()
		: m_bufferView()
		, m_isVertexStreamView(false)
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
		, m_bufferView(view)
	{
	}


	BufferInput::BufferInput(char const* shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
		: core::INamedObject(shaderName)
		, m_buffer(buffer)
		, m_bufferView(view)
	{
	}


	BufferInput::BufferInput(char const* shaderName, std::shared_ptr<re::Buffer> const& buffer)
		: core::INamedObject(shaderName)
		, m_buffer(buffer)
		, m_bufferView(buffer)
	{
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