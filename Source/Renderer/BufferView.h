// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "EnumTypes.h"
#include "VertexStream.h"

#include "Core/Interfaces/INamedObject.h"


namespace re
{
	class Buffer;


	class BufferView
	{
	public:
		struct BufferType
		{
			uint32_t m_firstElement = 0;		// Index of first array element to be accessed
			uint32_t m_numElements = 1;			// Number of array elements visible (i.e. structured buffers)
			uint32_t m_structuredByteStride;	// Structured buffer: Byte size of 1 struct/element. CBV: Size in bytes

			// TODO: This binding information probably shouldn't be part of the view, but it's convenient for now
			uint32_t m_firstDestIdx = 0;		// Shader-side arrays of Buffers: First slot to bind against
		};

		struct VertexStreamType
		{
			gr::VertexStream::Type m_type = gr::VertexStream::Type::Color;
			re::DataType m_dataType = re::DataType::DataType_Count;
			bool m_isNormalized = false;
			uint32_t m_numElements = 0;
		};


	public:
		union
		{
			BufferType m_buffer;
			VertexStreamType m_stream;
		};


	private:
		DataHash m_dataHash; // To sidestep headaches caused by our union, we manually handle our data hash


	public:
		BufferView(BufferType const&);
		BufferView(std::shared_ptr<re::Buffer> const&); // Infer a default view from the Buffer

		BufferView(VertexStreamType const&);


		BufferView(/* Don't use this directly */);


	public:
		~BufferView() = default;

		BufferView(BufferView const&) = default;
		BufferView(BufferView&&) noexcept = default;

		BufferView& operator=(BufferView const&) = default;
		BufferView& operator=(BufferView&&) noexcept = default;


	public:
		DataHash GetDataHash() const;
	};


	inline DataHash BufferView::GetDataHash() const
	{
		return m_dataHash;
	}


	// -----------------------------------------------------------------------------------------------------------------


	class BufferInput : public virtual core::INamedObject
	{
	public:
		BufferInput();

		BufferInput(char const* shaderName, std::shared_ptr<re::Buffer>, re::BufferView const&);
		BufferInput(std::string const& shaderName, std::shared_ptr<re::Buffer>, re::BufferView const&);
		
		// Infer a default view from the Buffer
		BufferInput(char const* shaderName, std::shared_ptr<re::Buffer>);
		BufferInput(std::string const& shaderName, std::shared_ptr<re::Buffer>);
		


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
		util::StringHash GetShaderNameHash() const;

		BufferView const& GetView() const;

		bool IsValid() const;
		void Release();


	private:
		std::shared_ptr<re::Buffer> m_buffer;
		BufferView m_view;
	};


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


	inline util::StringHash BufferInput::GetShaderNameHash() const
	{
		return GetNameHash();
	}


	inline BufferView const& BufferInput::GetView() const
	{
		return m_view;
	}


	inline bool BufferInput::IsValid() const
	{
		return m_buffer != nullptr;
	}
}