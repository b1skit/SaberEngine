// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "EnumTypes.h"
#include "VertexStream.h"

#include "Core/Assert.h"
#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"


namespace re
{
	class Buffer;


	class BufferView final
	{
	public:
		struct BufferType final
		{
			uint32_t m_firstElement = 0;		// Index of first array element to be accessed
			uint32_t m_numElements = 1;			// Number of array elements visible (i.e. structured buffers)
			uint32_t m_structuredByteStride;	// Structured buffer: Byte size of 1 struct/element. CBV: Size in bytes

			// TODO: This binding information probably shouldn't be part of the view, but it's convenient for now
			uint32_t m_firstDestIdx = 0;		// Shader-side arrays of Buffers: First element to bind against
		};

		struct VertexStreamType final
		{
			uint32_t m_firstElement = 0;		// Index of first vertex element to be accessed
			uint32_t m_numElements = 0;
			gr::VertexStream::Type m_type = gr::VertexStream::Type::Type_Count;
			re::DataType m_dataType = re::DataType::DataType_Count;
			bool m_isNormalized = false;
		};


	public:
		union
		{
			BufferType m_bufferView;
			VertexStreamType m_streamView;
		};


	private:
		util::HashKey m_dataHash; // To sidestep headaches caused by our union, we manually handle our data hash
		bool m_isVertexStreamView;


	public:
		BufferView(BufferType&&) noexcept;
		BufferView(BufferType const&);
		
		BufferView(std::shared_ptr<re::Buffer> const&); // Infer a default view from the Buffer

		BufferView(VertexStreamType&&) noexcept;
		BufferView(VertexStreamType const&);


	public:
		BufferView(/* Don't use this directly */);


	public:
		~BufferView() = default;

		BufferView(BufferView const&) = default;
		BufferView(BufferView&&) noexcept = default;

		BufferView& operator=(BufferView const&) = default;
		BufferView& operator=(BufferView&&) noexcept = default;


	public:
		util::HashKey GetDataHash() const;
		bool IsVertexStreamView() const;
	};


	inline util::HashKey BufferView::GetDataHash() const
	{
		return m_dataHash;
	}


	inline bool BufferView::IsVertexStreamView() const
	{
		return m_isVertexStreamView;
	}


	// -----------------------------------------------------------------------------------------------------------------


	class BufferInput final : public virtual core::INamedObject
	{
	public:
		BufferInput(); // Default/invalid view

		BufferInput(
			std::string_view shaderName,
			std::shared_ptr<re::Buffer>&&,
			re::BufferView&&,
			re::Lifetime viewLifetime);

		BufferInput(
			std::string_view shaderName,
			std::shared_ptr<re::Buffer>&&,
			re::BufferView const&,
			re::Lifetime viewLifetime);

		BufferInput(
			std::string_view shaderName,
			std::shared_ptr<re::Buffer> const&,
			re::BufferView&&,
			re::Lifetime viewLifetime);

		BufferInput(
			std::string_view shaderName,
			std::shared_ptr<re::Buffer> const&,
			re::BufferView const&,
			re::Lifetime viewLifetime); // Specify an equal or stricter lifetime than the Buffer's lifetime

		// Infer a default lifetime from the Buffer
		BufferInput(std::string_view shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView&&);
		BufferInput(std::string_view shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);

		// Infer a default view from the Buffer
		BufferInput(std::string_view shaderName, std::shared_ptr<re::Buffer> const&, re::Lifetime);

		// Infer a default view and lifetime from the Buffer
		BufferInput(std::string_view shaderName, std::shared_ptr<re::Buffer> const&);
		

	public:
		BufferInput(BufferInput const&) = default;
		BufferInput(BufferInput&&) noexcept = default;

		BufferInput& operator=(BufferInput const&) = default;
		BufferInput& operator=(BufferInput&&) noexcept = default;

		~BufferInput() = default;


	public:
		re::Buffer const* GetBuffer() const;
		re::Buffer* GetBuffer();
		
		std::string const& GetShaderName() const;
		util::HashKey GetShaderNameHash() const;

		BufferView const& GetView() const;

		re::Lifetime GetLifetime() const;

		bool IsValid() const;
		void Release();


	private:
		std::shared_ptr<re::Buffer> m_buffer;
		BufferView m_bufferView;
		re::Lifetime m_viewLifetime;
	};


	inline BufferInput::BufferInput()
		: core::INamedObject("Invalid_DefaultConstructedBufferInput")
		, m_buffer(nullptr)
		, m_viewLifetime(re::Lifetime::Permanent)
	{
	}


	inline BufferInput::BufferInput(
		std::string_view shaderName,
		std::shared_ptr<re::Buffer>&& buffer,
		re::BufferView&& view,
		re::Lifetime viewLifetime)
		: core::INamedObject(shaderName)
		, m_buffer(std::move(buffer))
		, m_bufferView(std::move(view))
		, m_viewLifetime(viewLifetime)
	{
		SEAssert(m_viewLifetime == m_buffer->GetLifetime() ||
			m_viewLifetime == re::Lifetime::SingleFrame && m_buffer->GetLifetime() == re::Lifetime::Permanent,
			"Incompatible BufferInput and Buffer lifetimes");
	}


	inline BufferInput::BufferInput(
		std::string_view shaderName,
		std::shared_ptr<re::Buffer>&& buffer,
		re::BufferView const& view,
		re::Lifetime viewLifetime)
		: BufferInput(shaderName, std::move(buffer), re::BufferView(view), viewLifetime)
	{
	}


	inline BufferInput::BufferInput(
		std::string_view shaderName,
		std::shared_ptr<re::Buffer> const& buffer,
		re::BufferView&& view,
		re::Lifetime viewLifetime)
		: BufferInput(shaderName, std::shared_ptr<re::Buffer>(buffer), std::move(view), viewLifetime)
	{
	}


	inline BufferInput::BufferInput(
		std::string_view shaderName,
		std::shared_ptr<re::Buffer> const& buffer,
		re::BufferView const& view,
		re::Lifetime viewLifetime)
		: BufferInput(shaderName, std::shared_ptr<re::Buffer>(buffer), re::BufferView(view), viewLifetime)
	{
	}


	inline BufferInput::BufferInput(
		std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView&& view)
		: BufferInput(shaderName, std::shared_ptr<re::Buffer>(buffer), std::move(view), buffer->GetLifetime())
	{
	}


	inline BufferInput::BufferInput(
		std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
		: BufferInput(shaderName, std::shared_ptr<re::Buffer>(buffer), re::BufferView(view), buffer->GetLifetime())
	{
	}


	inline BufferInput::BufferInput(
		std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer, re::Lifetime lifetime)
		: BufferInput(shaderName, buffer, re::BufferView(buffer), lifetime)
	{
	}


	inline BufferInput::BufferInput(std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer)
		: BufferInput(shaderName, buffer, re::BufferView(buffer), buffer->GetLifetime())
	{
	}


	inline void BufferInput::Release()
	{
		m_buffer = nullptr;
	}


	inline re::Buffer const* BufferInput::GetBuffer() const
	{
		return m_buffer.get();
	}


	inline re::Buffer* BufferInput::GetBuffer()
	{
		return m_buffer.get();
	}


	inline std::string const& BufferInput::GetShaderName() const
	{
		return GetName();
	}


	inline util::HashKey BufferInput::GetShaderNameHash() const
	{
		return GetNameHash();
	}


	inline BufferView const& BufferInput::GetView() const
	{
		return m_bufferView;
	}


	inline re::Lifetime BufferInput::GetLifetime() const
	{
		return m_viewLifetime;
	}


	inline bool BufferInput::IsValid() const
	{
		return m_buffer != nullptr;
	}


	// -----------------------------------------------------------------------------------------------------------------


	class VertexBufferInput final
	{
	public:
		static constexpr uint8_t k_invalidSlotIdx = std::numeric_limits<uint8_t>::max();
		
		VertexBufferInput();
		VertexBufferInput(core::InvPtr<gr::VertexStream> const&);
		VertexBufferInput(core::InvPtr<gr::VertexStream> const&, re::Buffer const* bufferOverride);

		VertexBufferInput(VertexBufferInput const&) noexcept = default;
		VertexBufferInput(VertexBufferInput&&) noexcept = default;
		VertexBufferInput& operator=(VertexBufferInput const&) noexcept = default;
		VertexBufferInput& operator=(VertexBufferInput&&) noexcept = default;
		~VertexBufferInput() = default;

	public:
		core::InvPtr<gr::VertexStream> const& GetStream() const;
		core::InvPtr<gr::VertexStream>& GetStream();

		re::Buffer const* GetBuffer() const;


	public:
		re::BufferView m_view;


	private:
		core::InvPtr<gr::VertexStream> m_vertexStream;
		re::Buffer const* m_bufferOverride;
	};


	inline VertexBufferInput::VertexBufferInput()
		: m_vertexStream()
		, m_bufferOverride(nullptr)
		, m_view(BufferView::VertexStreamType{})
	{
	}


	inline VertexBufferInput::VertexBufferInput(core::InvPtr<gr::VertexStream> const& stream)
		: m_vertexStream(stream)
		, m_bufferOverride(nullptr)
	{
		if (m_vertexStream)
		{
			m_view = re::BufferView::VertexStreamType{
				.m_firstElement = 0,
				.m_numElements = stream->GetNumElements(),
				.m_type = stream->GetType(),
				.m_dataType = stream->GetDataType(),
				.m_isNormalized = static_cast<bool>(stream->DoNormalize()),
			};
		}
		else
		{
			m_view = BufferView::VertexStreamType{};
		}
	}


	inline VertexBufferInput::VertexBufferInput(core::InvPtr<gr::VertexStream> const& stream, re::Buffer const* bufferOverride)
		: m_vertexStream(stream)
		, m_bufferOverride(bufferOverride)
	{
		SEAssert(m_vertexStream && m_bufferOverride, "Override constructure requires a valid stream and buffer");

		m_view = re::BufferView::VertexStreamType{
			.m_firstElement = 0,
			.m_numElements = stream->GetNumElements(),
			.m_type = stream->GetType(),
			.m_dataType = stream->GetDataType(),
			.m_isNormalized = static_cast<bool>(stream->DoNormalize()),			
		};
	}


	inline core::InvPtr<gr::VertexStream> const& VertexBufferInput::GetStream() const
	{
		return m_vertexStream;
	}


	inline core::InvPtr<gr::VertexStream>& VertexBufferInput::GetStream()
	{
		return m_vertexStream;
	}


	inline re::Buffer const* VertexBufferInput::GetBuffer() const
	{
		if (m_bufferOverride)
		{
			return m_bufferOverride;
		}
		return m_vertexStream->GetBuffer();
	}
}