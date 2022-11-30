#include "SceneData_OpenGL.h"
#include "Texture_OpenGL.h"


namespace opengl
{
	void opengl::SceneData::PostProcessLoadedData(fr::SceneData& sceneData)
	{
		for (auto& textureItr : sceneData.m_textures)
		{
			opengl::Texture::Create(*textureItr.second);
		}
	}
}