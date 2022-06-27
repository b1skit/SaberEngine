// Saber Engine texture object
// Contains everything needed to describe texture data

#include "Texture.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"
#include "Material.h"


#define STBI_FAILURE_USERMSG
#include <stb_image.h>				// STB image loader. No need to #define STB_IMAGE_IMPLEMENTATION, as it was already defined in SceneManager

#include <string>

using std::to_string;

#define ERROR_TEXTURE_NAME "RedErrorTexture"
#define DEFAULT_ALPHA 1.0f			// Default alpha value when loading texture data, if no alpha exists


namespace SaberEngine
{
	Texture::Texture()
	{
		m_numTexels				= m_width * m_height;
		m_texels					= new vec4[m_numTexels];	// Allocate the default size
		m_resolutionHasChanged	= true;
		
		Fill(TEXTURE_ERROR_COLOR_VEC4);
	}

	// Constructor:
	Texture::Texture(int width, int height, string texturePath, bool doFill /* = true */, vec4 fillColor /* = (1.0, 0.0, 0.0, 1.0) */)
	{
		m_width				= width;
		m_height			= height;
		m_numTexels			= width * height;

		m_texturePath		= texturePath;

		// Initialize the texture:
		m_texels					= new vec4[m_numTexels];
		m_resolutionHasChanged	= true;
		if (doFill)
		{
			Fill(fillColor);
		}
	}


	// Copy constructor:
	Texture::Texture(Texture const& rhs)
	{
		// Cleanup:
		if (m_texels != nullptr)
		{
			delete[] m_texels;
			m_texels				= nullptr;
			m_numTexels				= 0;
			m_resolutionHasChanged	= true;
		}

		// Copy properties:
		m_textureID				= 0;	// NOTE: This could potentially result in some textures never being destroyed (ie. If this is called on a non-duplicated texture) TODO: Investigate this

		m_texTarget				= rhs.m_texTarget;
		m_format				= rhs.m_format;
		m_internalFormat		= rhs.m_internalFormat;
		type					= rhs.type;

		m_textureWrapS			= rhs.m_textureWrapS;
		m_textureWrapT			= rhs.m_textureWrapT;
		m_textureWrapR			= rhs.m_textureWrapR;

		m_textureMinFilter		= rhs.m_textureMinFilter;
		m_textureMaxFilter		= rhs.m_textureMaxFilter;

		m_samplerID				= rhs.m_samplerID;

		m_width					= rhs.m_width;
		m_height				= rhs.m_height;

		m_numTexels				= rhs.m_numTexels;

		if (rhs.m_texels != nullptr && m_numTexels > 0)
		{
			m_texels				= new vec4[m_numTexels];
			m_resolutionHasChanged	= true;

			for (unsigned int i = 0; i < m_numTexels; i++)
			{
				m_texels[i] = rhs.m_texels[i];
			}
		}

		m_texturePath = rhs.m_texturePath;
	}


	void Texture::Destroy()
	{
		if (glIsTexture(m_textureID))
		{
			glDeleteTextures(1, &m_textureID);
		}

		if (m_texels != nullptr)
		{
			delete[] m_texels;
			m_texels = nullptr;
			m_numTexels = 0;
			m_resolutionHasChanged = true;
		}

		glDeleteSamplers(1, &m_samplerID);
	}


	Texture& SaberEngine::Texture::operator=(Texture const& rhs)
	{
		if (this == &rhs)
		{
			return *this;
		}

		// Cleanup:
		if (m_texels != nullptr)
		{
			Destroy();
		}


		// Copy properties:
		m_textureID			= 0;	// NOTE: Texture.Buffer() must be called before this texture can be used

		m_texTarget			= rhs.m_texTarget;
		m_format			= rhs.m_format;
		m_internalFormat	= rhs.m_internalFormat;
		type				= rhs.type;

		m_textureWrapS		= rhs.m_textureWrapS;
		m_textureWrapT		= rhs.m_textureWrapT;
		m_textureWrapR		= rhs.m_textureWrapR;

		m_textureMinFilter	= rhs.m_textureMinFilter;
		m_textureMaxFilter	= rhs.m_textureMaxFilter;

		m_samplerID			= rhs.m_samplerID;

		m_width				= rhs.m_width;
		m_height			= rhs.m_height;

		m_numTexels			= rhs.m_numTexels;

		if (rhs.m_texels != nullptr && m_numTexels > 0)
		{
			m_texels				= new vec4[m_numTexels];
			m_resolutionHasChanged	= true;

			for (unsigned int i = 0; i < m_numTexels; i++)
			{
				m_texels[i] = rhs.m_texels[i];
			}
		}

		m_texturePath = rhs.m_texturePath;

		return *this;
	}


	vec4& SaberEngine::Texture::Texel(unsigned int u, unsigned int v)
	{
		if (u >= m_width || v >= m_height)
		{
			LOG_ERROR("Invalid texture access! Cannot access texel (" + to_string(u) + ", " + to_string(v) + " in a texture of size " + to_string(m_width) + "x" + to_string(m_height));

			// Try and return the safest option:
			return m_texels[0];
		}

		return m_texels[(v * m_width) + u]; // Number of elements in v rows, + uth element in next row
	}


	void SaberEngine::Texture::Fill(vec4 color)
	{
		for (unsigned int row = 0; row < m_height; row++)
		{
			for (unsigned int col = 0; col < m_width; col++)
			{
				Texel(row, col) = color;
			}
		}
	}


	void SaberEngine::Texture::Fill(vec4 tl, vec4 tr, vec4 bl, vec4 br)
	{
		for (unsigned int row = 0; row < m_height; row++)
		{
			float vertDelta = (float)((float)row / (float)m_height);
			vec4 startCol = (vertDelta * bl) + ((1.0f - vertDelta) * tl);
			vec4 endCol = (vertDelta * br) + ((1.0f - vertDelta) * tr);

			for (unsigned int col = 0; col < m_width; col++)
			{
				float horDelta = (float)((float)col / (float)m_width);

				Texel(row, col) = (horDelta * endCol) + ((1.0f - horDelta) * startCol);
			}
		}
	}



	// Static functions:
	//------------------

	Texture* Texture::LoadTextureFileFromPath(string texturePath, bool returnErrorTexIfNotFound /*= false*/, bool flipY /*= true*/)
	{
		stbi_set_flip_vertically_on_load(flipY);	// Set stb_image to flip the y-axis on loading to match OpenGL's style (So pixel (0,0) is in the bottom-left of the image)

		LOG("Attempting to load texture \"" + texturePath + "\"");

		int width, height, numChannels;
		void* imageData = nullptr;
		bool isHDR		= false;

		// Handle HDR images:
		if (stbi_is_hdr(texturePath.c_str()))
		{
			imageData	= stbi_loadf(texturePath.c_str(), &width, &height, &numChannels, 0);
			isHDR		= true;
		}
		else
		{
			imageData = stbi_load(texturePath.c_str(), &width, &height, &numChannels, 0);
		}
		

		if (imageData)
		{
			LOG("Found " + to_string(width) + "x" + to_string(height) + (isHDR?" HDR ":" LDR ") + "texture with " + to_string(numChannels) + " channels");

			Texture* texture = new Texture(width, height, texturePath, false);

			if (isHDR)
			{
				float* castImageData = static_cast<float*>(imageData);
				LoadHDRHelper(*texture, castImageData, width, height, numChannels);
			}
			else
			{
				unsigned char* castImageData = static_cast<unsigned char*>(imageData);
				LoadLDRHelper(*texture, castImageData, width, height, numChannels);
			}

			// Cleanup:
			stbi_image_free(imageData);

			#if defined(DEBUG_SCENEMANAGER_TEXTURE_LOGGING)
				LOG("Completed loading texture: " + m_texturePath);
			#endif

			return texture;
		}
		else if (!returnErrorTexIfNotFound)
		{
			return nullptr;
		}

		// If we've made it this far, we couldn't load an image from a file:
		char const* failResult = stbi_failure_reason();
		LOG_ERROR("Could not load texture at \"" + texturePath + "\", error: \"" + string(failResult) + ".\" Returning solid red color!");

		width = height = 1;
		Texture* texture = new Texture(width, height, ERROR_TEXTURE_NAME, true, TEXTURE_ERROR_COLOR_VEC4);

		return texture;
	}


	void Texture::LoadLDRHelper(Texture& targetTexture, const unsigned char* imageData, int width, int height, int numChannels)
	{
		// Read texel values:
		const unsigned char* currentElement = imageData;
		for (int row = 0; row < height; row++)
		{
			for (int col = 0; col < width; col++)
			{
				vec4 currentPixel(0.0f, 0.0f, 0.0f, DEFAULT_ALPHA);

				for (int channel = 0; channel < numChannels; channel++)
				{
					currentPixel[channel] = (float)((float)((unsigned int)*currentElement) / 255.0f);
					currentElement++;
				}

				targetTexture.Texel(col, row) = currentPixel;
			}
		}
	}

	void Texture::LoadHDRHelper(Texture& targetTexture, const float* imageData, int width, int height, int numChannels)
	{
		// Read texel values:
		const float* currentElement = imageData;
		for (int row = 0; row < height; row++)
		{
			for (int col = 0; col < width; col++)
			{
				vec4 currentPixel(0.0f, 0.0f, 0.0f, DEFAULT_ALPHA);

				for (int channel = 0; channel < numChannels; channel++)
				{
					currentPixel[channel] = *currentElement;

					currentElement++;
				}

				targetTexture.Texel(col, row) = currentPixel;
			}
		}		
	}


	bool Texture::Buffer(int textureUnit)
	{
		LOG("Buffering texture: \"" + TexturePath() + "\"");

		glBindTexture(m_texTarget, m_textureID);

		// If the texture hasn't been created, create a new name:
		if (!glIsTexture(m_textureID))
		{
			#if defined(DEBUG_SCENEMANAGER_TEXTURE_LOGGING)
				LOG("Texture has never been bound before!");
			#endif

			glGenTextures(1, &m_textureID);
			glBindTexture(m_texTarget, m_textureID);
			if (glIsTexture(m_textureID) != GL_TRUE)
			{
				LOG_ERROR("OpenGL failed to generate new texture name. Texture buffering failed");
				glBindTexture(m_texTarget, 0);
				return false;
			}

			// UV wrap mode:
			glTexParameteri(m_texTarget, GL_TEXTURE_WRAP_S, m_textureWrapS);	// u
			glTexParameteri(m_texTarget, GL_TEXTURE_WRAP_T, m_textureWrapT);	// v

			// Mip map min/maximizing:
			glTexParameteri(m_texTarget, GL_TEXTURE_MIN_FILTER, m_textureMinFilter);
			glTexParameteri(m_texTarget, GL_TEXTURE_MAG_FILTER, m_textureMaxFilter);
		}
		#if defined(DEBUG_SCENEMANAGER_TEXTURE_LOGGING)
		else
		{
			LOG("Found existing texture");
		}
		#endif


		// Upload to the GPU:
		if (m_texels != nullptr) // I.e. Texture:
		{
			if (m_resolutionHasChanged)
			{
				#if defined(DEBUG_SCENEMANAGER_TEXTURE_LOGGING)
					LOG("Buffering texture values");
				#endif

				// Compute storage properties for our texture:
				int largestDimension = glm::max(m_width, m_height);
				int numMipLevels = (int)glm::log2((float)largestDimension) + 1;

				glTexStorage2D(m_texTarget, numMipLevels, m_internalFormat, m_width, m_height);

				m_resolutionHasChanged = false;
			}

			glTexSubImage2D(m_texTarget, 0, 0, 0, m_width, m_height, m_format, type, &Texel(0, 0).r);
			//glTexImage2D(texTarget, 0, internalFormat, width, height, 0, format, type, &Texel(0, 0).r); // Won't work if glTexStorage2D has been called

			glGenerateMipmap(m_texTarget);

			#if defined(DEBUG_SCENEMANAGER_TEXTURE_LOGGING)
				LOG("Texture buffering complete!");
			#endif

			// Cleanup:
			glBindTexture(m_texTarget, 0);
		}
		else // I.e. RenderTexture:
		{			
			if (m_resolutionHasChanged)	// We don't really care about the resolution of render textures changing, for now...
			{
				m_resolutionHasChanged = false;
			}
			glTexImage2D(m_texTarget, 0, m_internalFormat, m_width, m_height, 0, m_format, type, nullptr);

			// Note: We don't unbind the texture here so RenderTexture::Buffer() doesn't have to rebind it
		}

		// Configure the Texture sampler:
		glBindSampler(textureUnit, m_samplerID);
		if (!glIsSampler(m_samplerID))
		{
			glGenSamplers(1, &m_samplerID);
			glBindSampler(textureUnit, m_samplerID);
		}

		glSamplerParameteri(m_samplerID, GL_TEXTURE_WRAP_S, m_textureWrapS);
		glSamplerParameteri(m_samplerID, GL_TEXTURE_WRAP_T, m_textureWrapT);				

		glSamplerParameteri(m_samplerID, GL_TEXTURE_MIN_FILTER, m_textureMinFilter);
		glSamplerParameteri(m_samplerID, GL_TEXTURE_MAG_FILTER, m_textureMaxFilter);

		glBindSampler(textureUnit, 0);

		return true;
	}


	bool Texture::BufferCubeMap(Texture** cubeFaces, int textureUnit) // Note: There must be EXACTLY 6 elements in cubeFaces
	{
		// NOTE: This function uses the paramters of cubeFaces[0]

		LOG("Buffering cube map: \"" + cubeFaces[0]->TexturePath() + "\"");

		// Bind Texture:
		glBindTexture(cubeFaces[0]->m_texTarget, cubeFaces[0]->m_textureID);
		if (!glIsTexture(cubeFaces[0]->m_textureID))
		{
			glGenTextures(1, &cubeFaces[0]->m_textureID);
			glBindTexture(cubeFaces[0]->m_texTarget, cubeFaces[0]->m_textureID);

			if (!glIsTexture(cubeFaces[0]->m_textureID))
			{
				LOG_ERROR("OpenGL failed to generate new cube map texture name. Texture buffering failed");
				glBindTexture(cubeFaces[0]->m_texTarget, 0);
				return false;
			}
		}

		// Set texture params:
		glTexParameteri(cubeFaces[0]->m_texTarget, GL_TEXTURE_WRAP_S, cubeFaces[0]->m_textureWrapS);
		glTexParameteri(cubeFaces[0]->m_texTarget, GL_TEXTURE_WRAP_T, cubeFaces[0]->m_textureWrapT);
		glTexParameteri(cubeFaces[0]->m_texTarget, GL_TEXTURE_WRAP_R, cubeFaces[0]->m_textureWrapR);

		glTexParameteri(cubeFaces[0]->m_texTarget, GL_TEXTURE_MAG_FILTER, cubeFaces[0]->m_textureMaxFilter);
		glTexParameteri(cubeFaces[0]->m_texTarget, GL_TEXTURE_MIN_FILTER, cubeFaces[0]->m_textureMinFilter);

		// Bind sampler:
		glBindSampler(textureUnit, cubeFaces[0]->m_samplerID);
		if (!glIsSampler(cubeFaces[0]->m_samplerID))
		{
			glGenSamplers(1, &cubeFaces[0]->m_samplerID);
			glBindSampler(textureUnit, cubeFaces[0]->m_samplerID);

			if (!glIsSampler(cubeFaces[0]->m_samplerID))
			{
				LOG_ERROR("Could not create cube map sampler");
				return false;
			}
		}

		// Set sampler params:
		glSamplerParameteri(cubeFaces[0]->m_samplerID, GL_TEXTURE_WRAP_S, cubeFaces[0]->m_textureWrapS);
		glSamplerParameteri(cubeFaces[0]->m_samplerID, GL_TEXTURE_WRAP_T, cubeFaces[0]->m_textureWrapT);

		glSamplerParameteri(cubeFaces[0]->m_samplerID, GL_TEXTURE_MIN_FILTER, cubeFaces[0]->m_textureMinFilter);
		glSamplerParameteri(cubeFaces[0]->m_samplerID, GL_TEXTURE_MAG_FILTER, cubeFaces[0]->m_textureMaxFilter);

		glBindSampler(textureUnit, 0);


		// Texture cube map specific setup:
		if (cubeFaces[0]->m_texels != nullptr)
		{
			// Generate faces:
			for (int i = 0; i < CUBE_MAP_NUM_FACES; i++)
			{
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, cubeFaces[0]->m_internalFormat, cubeFaces[0]->m_width, cubeFaces[0]->m_height, 0, cubeFaces[0]->m_format, cubeFaces[0]->type, &cubeFaces[i]->Texel(0, 0).r);
			}


			// Ensure all of the textures have the correct information stored in them:
			for (int i = 1; i < CUBE_MAP_NUM_FACES; i++)
			{
				cubeFaces[i]->m_textureID = cubeFaces[0]->m_textureID;
				cubeFaces[i]->m_samplerID = cubeFaces[0]->m_samplerID;
			}

			// Cleanup:
			glBindTexture(cubeFaces[0]->m_texTarget, 0); // Otherwise, we leave the texture bound for the remaining RenderTexture BufferCubeMap()
		}

		return true;
	}


	void Texture::Bind(int textureUnit, bool doBind)
	{
		glActiveTexture(GL_TEXTURE0 + textureUnit);

		// Handle unbinding:
		if (doBind == false)
		{
			glBindTexture(m_texTarget, 0);
			glBindSampler(textureUnit, 0); // Assign to index/unit 0

		}
		else // Handle binding:
		{			
			glBindTexture(m_texTarget, m_textureID);
			glBindSampler(textureUnit, m_samplerID); // Assign our named sampler to the texture
		}
	}


	vec4 Texture::TexelSize()
	{
		// Check: Have the dimensions changed vs what we have cached?
		if (m_texelSize.z != m_width || m_texelSize.w != m_height)
		{
			m_texelSize = vec4(1.0f / m_width, 1.0f / m_height, m_width, m_height);
		}

		return m_texelSize;
	}


	void Texture::GenerateMipMaps()
	{
		glBindTexture(m_texTarget, m_textureID);
		glGenerateMipmap(m_texTarget);
		glBindTexture(m_texTarget, 0);
	}
}


