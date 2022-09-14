#include "Texture.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"

using glm::vec4;


namespace gr
{
	Texture::Texture(TextureParams params) :
		m_texParams{ params },
		m_texels(params.m_faces * params.m_width * params.m_height, params.m_clearColor),
		m_isCreated{ false },
		m_isDirty{ true },
		m_platformParams{ nullptr } // Initialized during Create(), to ensure the texture is correclty configured
	{
	}


	void gr::Texture::Create()
	{
		// Note: Textures are shared, so duplicate Create() calls can/do happen. Simplest solution is to just abort here
		if (m_isCreated)
		{
			return;
		}

		platform::Texture::Create(*this);
		m_isDirty = false;
		m_isCreated = true;

		return;
	}


	void gr::Texture::Bind(uint32_t textureUnit, bool doBind) const
	{
		platform::Texture::Bind(*this, textureUnit, doBind);
	}


	void Texture::Destroy()
	{
		if (m_texels.size() > 0)
		{
			m_texels.clear();
		}
		m_isCreated = false;
		m_isDirty = true;

		platform::Texture::Destroy(*this);

		m_platformParams = nullptr;
	}


	vec4 const& gr::Texture::GetTexel(uint32_t u, uint32_t v, uint32_t faceIdx/*= 0*/) const
	{
		SEAssert("OOB texel coordinates",
			u >= 0 && 
			u < m_texParams.m_width && 
			v >= 0 && 
			v < m_texParams.m_height && 
			faceIdx < m_texParams.m_faces);

		// Number of elements in v rows, + uth element in next row
		return m_texels[(faceIdx * m_texParams.m_width * m_texParams.m_height) + (v * m_texParams.m_width) + u];
	}


	glm::vec4 const& gr::Texture::GetTexel(uint32_t index) const
	{
		SEAssert("OOB texel coordinates", index < (m_texParams.m_faces * m_texParams.m_width * m_texParams.m_height));
		return m_texels[index];
	}


	void gr::Texture::SetTexel(uint32_t u, uint32_t v, glm::vec4 value)
	{
		SEAssert("OOB texel coordinates", u >= 0 && u < m_texParams.m_width&& v >= 0 && v < m_texParams.m_height);

		m_texels[(v * m_texParams.m_width) + u] = value;
		m_isDirty = true;
	}


	void gr::Texture::Fill(vec4 solidColor)
	{
		for (uint32_t row = 0; row < m_texParams.m_height; row++)
		{
			for (uint32_t col = 0; col < m_texParams.m_width; col++)
			{
				SetTexel(row, col, solidColor);
			}
		}
		m_isDirty = true;
	}


	void gr::Texture::Fill(vec4 tl, vec4 tr, vec4 bl, vec4 br)
	{
		for (unsigned int row = 0; row < m_texParams.m_height; row++)
		{
			float vertDelta = (float)((float)row / (float)m_texParams.m_height);
			vec4 startCol = (vertDelta * bl) + ((1.0f - vertDelta) * tl);
			vec4 endCol = (vertDelta * br) + ((1.0f - vertDelta) * tr);

			for (unsigned int col = 0; col < m_texParams.m_width; col++)
			{
				float horDelta = (float)((float)col / (float)m_texParams.m_width);

				SetTexel(row, col, (horDelta * endCol) + ((1.0f - horDelta) * startCol));
			}
		}
		m_isDirty = true;
	}


	vec4 Texture::GetTexelDimenions() const
	{
		// .xyzw = 1/width, 1/height, width, height
		return glm::vec4(
			1.0f / m_texParams.m_width, 
			1.0f / m_texParams.m_height, 
			m_texParams.m_width, 
			m_texParams.m_height);
	}


	uint32_t Texture::GetNumMips() const
	{
		if (!m_texParams.m_useMIPs)
		{
			return 1;
		}

		const uint32_t largestDimension = glm::max(m_texParams.m_width, m_texParams.m_height);
		return (uint32_t)glm::log2((float)largestDimension) + 1;
	}

	uint32_t Texture::GetMipDimension(uint32_t mipLevel) const
	{
		// No reason we can't support non-square textures, but until we need to just assert
		SEAssert("Dimensions mismatch but (currently) assuming square texture", Width() == Height());
		return (uint32_t)(Width() / glm::pow(2.0f, mipLevel));
	}
}


