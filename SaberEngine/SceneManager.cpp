#include "BuildConfiguration.h"
#include "SceneManager.h"
#include "EventManager.h"
#include "CoreEngine.h"
#include "Camera.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"
#include "SceneObject.h"
#include "GameObject.h"
#include "PlayerObject.h"
#include "ImageBasedLight.h"
#include "ShadowMap.h"
#include "Material.h"
#include "Renderable.h"
#include "Light.h"
#include "Skybox.h"
#include "Scene.h"
#include "Shader.h"


#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

using glm::pi;


#include <assimp/Importer.hpp>		// Importer interface
#include <assimp/postprocess.h>		// Post processing flags


#define STB_IMAGE_IMPLEMENTATION	// Only include this define ONCE in the project
#include <stb_image.h>				// STB image loader


#include <algorithm>
#include <string>
#include <stdio.h>

#define INVALID_TEXTURE_PATH "InvalidTexturePath"


namespace SaberEngine
{
	SceneManager::SceneManager() : EngineComponent("SceneManager")
	{
		
	}


	SceneManager& SceneManager::Instance()
	{
		static SceneManager* instance = new SceneManager();
		return *instance;
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");

		m_currentScene = nullptr;
		

		// Scene manager cleanup:
		if (m_materials.size() > 0)
		{
			for (std::pair<string, std::shared_ptr<Material>> currentMaterialEntry : m_materials)
			{
				currentMaterialEntry.second = nullptr;
			}
			m_materials.clear();
		}

		// Texture cleanup:
		for (std::pair<string, std::shared_ptr<gr::Texture>> currentTexture : m_textures)
		{
			if (currentTexture.second != nullptr)
			{
				currentTexture.second = nullptr;
			}
		}
		m_textures.clear();
	}


	void SceneManager::Update()
	{
		for (int i = 0; i < (int)m_currentScene->m_gameObjects.size(); i++)
		{
			m_currentScene->m_gameObjects.at(i)->Update();
		}
	}


	void SceneManager::HandleEvent(EventInfo const * eventInfo)
	{
		return;
	}


	bool SceneManager::LoadScene(string sceneName)
	{
		if (sceneName == "")
		{
			LOG_ERROR("Quitting! No scene name received. Did you forget to use the \"-scene theSceneName\" command line argument?");
			CoreEngine::GetEventManager()->Notify(new EventInfo{ EVENT_ENGINE_QUIT, this, nullptr });
			return false;
		}

		LOG("Unloading existing scene");
		m_currentScene = nullptr;

		m_currentScene = std::make_shared<Scene>(sceneName);

		// Assemble paths:
		string sceneRoot = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("sceneRoot") + sceneName + "\\";
		string fbxPath = sceneRoot + sceneName + ".fbx";

		// Load our .fbx using Assimp:
		Assimp::Importer importer;
		aiScene const* scene = importer.ReadFile(fbxPath, 
			aiProcess_ValidateDataStructure 
			| aiProcess_CalcTangentSpace
			| aiProcess_Triangulate
			| aiProcess_JoinIdenticalVertices
			| aiProcess_SortByPType 
			| aiProcess_GenUVCoords 
			| aiProcess_TransformUVCoords
		); // | aiProcess_OptimizeMeshes | aiProcess_RemoveRedundantMaterials

		if (!scene)
		{
			CoreEngine::GetEventManager()->Notify(new EventInfo{ EVENT_ENGINE_QUIT, nullptr, new string("Failed to load scene file: " + fbxPath + ": " + importer.GetErrorString() ) });
			return false;
		}
		else
		{
			LOG("Successfully loaded scene file " + fbxPath);
		}
		

		// Extract materials and textures:
		//--------------------------------
		if (scene->HasTextures()) // Check for embedded textures
		{
			int numTextures = scene->mNumTextures;
			LOG_ERROR("Found " + to_string(numTextures) + " embedded scene textures. These will NOT be loaded!");
		}
		#if defined(DEBUG_SCENEMANAGER_MATERIAL_LOGGING)
			else
			{
				LOG("Scene has no embedded textures");
			}
		#endif

		if (scene->HasMaterials())
		{
			ImportMaterialsAndTexturesFromScene(scene, sceneName);
		}
		else
		{
			LOG_ERROR("Scene has no materials");
		}


		// Extract meshes:
		//----------------
		if (scene->HasMeshes())
		{
			ImportGameObjectGeometryFromScene(scene);
		}
		else
		{
			LOG_ERROR("Scene has no meshes");
		}


		// Setup skybox:
		//--------------
		ImportSky(sceneName);

		// Assemble material mesh lists:
		// -----------------------------
		AssembleMaterialMeshLists();


		//// DEBUG: Add a test mesh:
		//std::shared_ptr<Mesh> sphere = std::make_shared<Mesh>(Mesh::CreateSphere(5.0));
		//GameObject* sphereGameObject = new GameObject("sphereTest");

		//AddGameObject(sphereGameObject);
		//sphereGameObject->GetRenderable()->AddViewMeshAsChild(sphere);
		//currentScene->AddMesh(sphere);


		//std::shared_ptr<Material> sphereMaterial = GetMaterial("brick_phongShader");
		////^^ Need a GetLoadMaterial() function?
		//int materialIndex = AddMaterial(sphereMaterial);

		//materialMeshLists.at(materialIndex).emplace_back(sphere);


		////std::shared_ptr<Material> sphereMaterial = new Material("sphereMat", "lambertShader");
		//////^^ Need a GetLoadMaterial() function?
		////int materialIndex = AddMaterial(sphereMaterial);
		////
		////materialMeshLists.emplace_back(vector<std::shared_ptr<Mesh>>());
		////materialMeshLists.at(materialIndex).emplace_back(sphere);


		//// TODO: Revise GameObject/Mesh creation... Simplify!!!!



		// Extract lights:
		//----------------
		if (scene->HasLights())
		{
			ImportLightsFromScene(scene);
		}
		else
		{
			LOG_ERROR("Scene has no lights");
		}


		//// DEBUG: Add a test light:
		//vec3 lightColor(1.0f, 0.0f, 0.0f);
		//std::shared_ptr<Light> pointLight = std::make_shared<Light>("pointLight", LIGHT_POINT, lightColor, nullptr);

		//currentScene->AddLight(pointLight);
		//// NOTE: Currently, the light has a hard-coded radius of 5



		// Extract cameras:
		//-----------------
		if (scene->HasCameras())
		{
			ImportCamerasFromScene(scene);
		}
		else
		{
			LOG_WARNING("Scene has no camera");
			ImportCamerasFromScene();
		}
			   

		// Create a PlayerObject:
		//-----------------------
		PlayerObject* player = new PlayerObject(m_currentScene->GetMainCamera());
		m_currentScene->m_gameObjects.push_back(player);	
		LOG("Created PlayerObject using mainCamera");

		return true;
	}


	unordered_map<string, std::shared_ptr<Material>> const& SaberEngine::SceneManager::GetMaterials() const
	{
		return m_materials;
	}


	std::shared_ptr<Material> SceneManager::GetMaterial(string materialName)
	{
		auto result = m_materials.find(materialName);
		if (result != m_materials.end())
		{
			return result->second;
		}
		else
		{
			LOG_ERROR("Could not find material \"" + materialName + "\"");
			return nullptr;
		}
	}


	vector<std::shared_ptr<gr::Mesh>> const* SceneManager::GetRenderMeshes(std::shared_ptr<Material> targetMaterial)
	{
		// If materialIndex is out of bounds, return ALL meshes
		if (targetMaterial == nullptr)
		{
			return &m_currentScene->GetMeshes();
		}

		auto result = m_materialMeshLists.find(targetMaterial->Name());
		if (result == m_materialMeshLists.end())
		{
			return &m_currentScene->GetMeshes();
		}

		return &result->second;
	}


	vector<Renderable*>* SceneManager::GetRenderables()
	{ 
		return &m_currentScene->m_renderables; 
	}


	std::shared_ptr<Light> const& SceneManager::GetAmbientLight()
	{ 
		return m_currentScene->m_ambientLight; 
	}


	std::shared_ptr<Light> SceneManager::GetKeyLight()
	{ 
		return m_currentScene->m_keyLight; 
	}


	vector<std::shared_ptr<Camera>> const& SceneManager::GetCameras(CAMERA_TYPE cameraType)
	{ 
		return m_currentScene->GetCameras(cameraType); 
	}


	std::shared_ptr<Camera> SceneManager::GetMainCamera()
	{ 
		return m_currentScene->GetMainCamera(); 
	}


	void SceneManager::RegisterCamera(CAMERA_TYPE cameraType, std::shared_ptr<Camera> newCamera)
	{ 
		m_currentScene->RegisterCamera(cameraType, newCamera); 
	}


	void SceneManager::AddTexture(std::shared_ptr<gr::Texture>& newTexture)
	{
		if (newTexture == nullptr)
		{
			LOG_ERROR("Cannot add null texture to textures table");
			return;
		}

		// Check if the texture already exists:
		unordered_map<string, std::shared_ptr<gr::Texture>>::const_iterator texturePosition =
			m_textures.find(newTexture->GetTexturePath());
		if (texturePosition != m_textures.end())
		{
			LOG_WARNING("Cannot add texture with an identical path. Deleting duplicate, and updating reference");

			newTexture = nullptr;

			newTexture = texturePosition->second;
		}
		else // Insert the new texture:
		{
			m_textures[newTexture->GetTexturePath()] = newTexture;
		}
	}
	
	
	vector<std::shared_ptr<Light>> const& SceneManager::GetDeferredLights()
	{
		return m_currentScene->GetDeferredLights();
	}


	std::shared_ptr<Skybox> SceneManager::GetSkybox()
	{ 
		return m_currentScene->m_skybox; 
	}


	string const& SceneManager::GetCurrentSceneName() const
	{
		return m_currentScene->GetSceneName();
	}


	void SceneManager::AddGameObject(GameObject* newGameObject)
	{
		m_currentScene->m_gameObjects.push_back(newGameObject);

		// Store a pointer to the GameObject's Renderable and add it to the list for the RenderManager
		m_currentScene->m_renderables.push_back(newGameObject->GetRenderable());

		#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
			LOG("Added new GameObject to the scene: " + newGameObject->GetName());
		#endif	
	}


	void SceneManager::InitializeTransformValues(aiMatrix4x4 const& source, Transform* dest)
	{
		aiVector3D sourceScale, sourcePosition;
		aiQuaternion sourceRotation;
		source.Decompose(sourceScale, sourceRotation, sourcePosition); // Decompose the source matrix into its scale, rotation, position components

		#if defined(DEBUG_TRANSFORMS)
			LOG("Decomposed aiMatrix4x4 into:");
			LOG("\tSource Scale:    " + to_string(sourceScale.x) + " " + to_string(sourceScale.y) + " " + to_string(sourceScale.z));
			LOG("\tSource Position: " + to_string(sourcePosition.x) + " " + to_string(sourcePosition.y) + " " + to_string(sourcePosition.z));
			LOG("\tSource Rotation: " + to_string(sourceRotation.x) + " " + to_string(sourceRotation.y) + " " + to_string(sourceRotation.z) + " " + to_string(sourceRotation.w));
		#endif

		glm::quat sourceRotationAsGLMQuat(sourceRotation.w, sourceRotation.x, sourceRotation.y, sourceRotation.z);
		vec3 eulerRotation = glm::eulerAngles(sourceRotationAsGLMQuat);

		dest->SetWorldPosition(vec3(sourcePosition.x, sourcePosition.y, sourcePosition.z));
		dest->SetWorldRotation(sourceRotationAsGLMQuat);
		dest->SetWorldScale(vec3(sourceScale.x, sourceScale.y, sourceScale.z));
	}


	void SaberEngine::SceneManager::InitializeLightTransformValues(aiScene const* scene, string lightName, Transform* targetLightTransform)
	{
		aiMatrix4x4 lightTransform;
		aiNode* current = nullptr;
		if (current = scene->mRootNode->FindNode(lightName.c_str()))
		{
			#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
				LOG("Found a corresponding light node in the scene graph...");
			#endif

			lightTransform = GetCombinedTransformFromHierarchy(scene, current->mParent);
			lightTransform = lightTransform * current->mTransformation;	

			#if defined(DEBUG_SCENEMANAGER_TRANSFORM_LOGGING)
				LOG("-> " + string(current->mName.C_Str()) + " (Light's first transformation node)");
				LOG(to_string(current->mTransformation.a1) + " " + to_string(current->mTransformation.a2) + " " + to_string(current->mTransformation.a3) + " " + to_string(current->mTransformation.a4) );
				LOG(to_string(current->mTransformation.b1) + " " + to_string(current->mTransformation.b2) + " " + to_string(current->mTransformation.b3) + " " + to_string(current->mTransformation.b4));
				LOG(to_string(current->mTransformation.c1) + " " + to_string(current->mTransformation.c2) + " " + to_string(current->mTransformation.c3) + " " + to_string(current->mTransformation.c4));
				LOG(to_string(current->mTransformation.d1) + " " + to_string(current->mTransformation.d2) + " " + to_string(current->mTransformation.d3) + " " + to_string(current->mTransformation.d4));
			#endif
		}
		else
		{
			LOG_ERROR("Could not find a node matching light name \"" + lightName + "\" in the scene!");
		}

		InitializeTransformValues(lightTransform, targetLightTransform);
	}


	void SceneManager::AddMaterial(std::shared_ptr<Material>& newMaterial)
	{
		if (newMaterial == nullptr)
		{
			LOG_ERROR("Cannot add null material to scene manager material list");
			return;
		}

		auto result = m_materials.find(newMaterial->Name());

		// Add new Material
		if (result == m_materials.end())
		{
			m_materials[newMaterial->Name()] = newMaterial;
			LOG("Material \"" + newMaterial->Name() + "\" registered with SceneManager");
		}
		else // Material already exists: Destroy the duplicate and update the pointer
		{
			LOG_WARNING("The material \"" + newMaterial->Name() + "\" already exists! Destroying duplicate, and "
				"updating reference to point to original material");

			newMaterial = result->second;
		}
	}


	void SceneManager::AssembleMaterialMeshLists()
	{
		// TODO: Tune this value based on the actual number of meshes loaded?
		const unsigned int ESTIMATED_MESHES_PER_MATERIAL = 25;	

		unsigned int numMeshes = 0;
		for (int i = 0; i < (int)m_currentScene->m_renderables.size(); i++)
		{
			for (int j = 0; j < (int)m_currentScene->m_renderables.at(i)->ViewMeshes()->size(); j++)
			{
				std::shared_ptr<gr::Mesh> viewMesh = m_currentScene->m_renderables.at(i)->ViewMeshes()->at(j);

				std::shared_ptr<Material> meshMaterial = viewMesh->MeshMaterial();
				if (meshMaterial == nullptr)
				{
					LOG_ERROR("AssembleMaterialMeshLists() is skipping a mesh with NULL material pointer!");
				}
				else
				{
					auto result = m_materialMeshLists.find(meshMaterial->Name());

					if (result == m_materialMeshLists.end())
					{
						// Create a new entry, containing a vector with our object
						m_materialMeshLists[meshMaterial->Name()] = vector<std::shared_ptr<gr::Mesh>>{viewMesh};
					}
					else
					{
						result->second.emplace_back(viewMesh);
						numMeshes++;
					}					
				}
			}
		}

		LOG("\nAssembled material mesh list of " + to_string(numMeshes) + " meshes and " + to_string(m_materialMeshLists.size()) + " materials");
	}


	std::shared_ptr<gr::Texture> SaberEngine::SceneManager::FindLoadTextureByPath(
		string texturePath,
		gr::Texture::TextureColorSpace colorSpace,
		bool loadIfNotFound /*= true*/)
	{
		// NOTE: Potential bug here: Since we store textureUnit per-texture, we can only share textures that live in the
		// same slot. TODO: Move texture units into the Material?

		unordered_map<string, std::shared_ptr<gr::Texture>>::const_iterator texturePosition = m_textures.find(texturePath);
		if (texturePosition != m_textures.end())
		{
			LOG("Texture at path " + texturePath + " has already been loaded");
			return texturePosition->second;
		}

		// If we've made it this far, load the texture
		if (loadIfNotFound)
		{
			std::shared_ptr<gr::Texture> result(nullptr);
			bool didLoad = gr::Texture::LoadTextureFileFromPath(result, texturePath, colorSpace, false);
			if (didLoad)
			{
				AddTexture(result);
			}
			return result;
		}
		else
		{
			return nullptr;
		}
	}


	void SceneManager::ImportMaterialsAndTexturesFromScene(aiScene const* scene, string sceneName)
	{
		int numMaterials = scene->mNumMaterials;
		LOG("\nFound " + to_string(numMaterials) + " scene materials:");

		// Create Saber Engine materials:
		for (int currentMaterial = 0; currentMaterial < numMaterials; currentMaterial++)
		{
			// Get the material name:
			aiString name;
			if (AI_SUCCESS == scene->mMaterials[currentMaterial]->Get(AI_MATKEY_NAME, name))
			{
				string matName = string(name.C_Str());
				LOG("\nLoading scene material " + to_string(currentMaterial) + ": \"" + matName + "\"...");

				#if defined(DEBUG_SCENEMANAGER_MATERIAL_LOGGING)
					LOG("Printing received material property keys:");
					aiMaterial* currentAiMaterial = scene->mMaterials[currentMaterial];
					for (unsigned int i = 0; i < currentAiMaterial->mNumProperties; i++)
					{
						LOG("KEY: " + string(currentAiMaterial->mProperties[i]->mKey.C_Str()));
					}
				#endif

				// Create a material using the error shader, for now:
				std::shared_ptr<Material> newMaterial = std::make_shared<Material>(matName, std::shared_ptr<Shader>(nullptr));
				
				// Extract textures, and add them to the material:

				/* NOTE: For simplicity, SaberEngine interprets Phong shaders (only) loaded from FBX files:
					Shader name:	Attempt to use whatever follows the last _underscore as a shader name (Eg. myMaterial_phong)
					Albedo:			Phong's color (rgb)
					Transparency:	Phong's color (a)
					Normal:			Phong's bump (rgb)
					Emissive:		Phong's incandescence (rgb)

					Packed Roughness + Metalic + AO channels (RMAO):
						Roughness:		Phong's specular color (r)
						Metalic:		Phong's specular color (g)
						AO:				Phong's specular color (b)
					
					Packed material property 0 (RGBA):
					Phong Exponent: Phong's "Cosine Power" slot
					F0 Property:	Phong's "Reflected Color" slot
				*/

				// Extract material's textures:
				LOG("Importing albedo + transparency texture (RGB+A) from material's diffuse/color slot");

				std::shared_ptr<gr::Texture> diffuseTexture = 
					ExtractLoadTextureFromAiMaterial(aiTextureType_DIFFUSE, scene->mMaterials[currentMaterial], sceneName);
				if (diffuseTexture)
				{
					gr::Texture::TextureParams diffuseParams = diffuseTexture->GetTextureParams();

					diffuseParams.m_texColorSpace = gr::Texture::TextureColorSpace::sRGB;
					diffuseParams.m_texFormat = gr::Texture::TextureFormat::RGBA8;
					diffuseTexture->SetTextureParams(diffuseParams);


					newMaterial->AccessTexture(TEXTURE_ALBEDO) = diffuseTexture;
				}
				else
				{
					newMaterial->AddShaderKeyword(Shader::SHADER_KEYWORDS[NO_ALBEDO_TEXTURE]);
				}

				LOG("Importing normal map texture (RGB) from material's bump slot");
				std::shared_ptr<gr::Texture> normalTexture =
					ExtractLoadTextureFromAiMaterial(aiTextureType_NORMALS, scene->mMaterials[currentMaterial], sceneName);

				if (normalTexture)
				{
					gr::Texture::TextureParams normalParams = normalTexture->GetTextureParams();
					normalParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear;
					normalParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F;
					normalTexture->SetTextureParams(normalParams);

					newMaterial->AccessTexture(TEXTURE_NORMAL) = normalTexture;
				}
				else
				{
					// NOTE: This NEVER gets hit, since ExtractLoadTextureFromAiMaterial() will always assign a default 
					// 1x1 normal texture.... 
					// TODO: handle this more elegantly
					newMaterial->AddShaderKeyword(Shader::SHADER_KEYWORDS[NO_NORMAL_TEXTURE]);
				}
				

				LOG("Importing emissive map texture (RGB) from material's incandescence slot");

				std::shared_ptr<gr::Texture>  emissiveTexture = 
					ExtractLoadTextureFromAiMaterial(aiTextureType_EMISSIVE, scene->mMaterials[currentMaterial], sceneName);
				if (emissiveTexture)
				{
					gr::Texture::TextureParams emissiveParams = emissiveTexture->GetTextureParams();
					//emissiveParams.m_texColorSpace = gr::Texture::TextureColorSpace::sRGB; 
					emissiveParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear; // TODO: Are emissive textures sRGB or Linear????
					emissiveParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F;
					emissiveTexture->SetTextureParams(emissiveParams);

					newMaterial->AccessTexture(TEXTURE_EMISSIVE) = emissiveTexture;
				}
				else
				{
					newMaterial->AddShaderKeyword(Shader::SHADER_KEYWORDS[NO_EMISSIVE_TEXTURE]);
				}

				LOG("Importing roughness, metalic, & AO textures (R+G+B) from material's specular slot");

				std::shared_ptr<gr::Texture> RMAO = 
					ExtractLoadTextureFromAiMaterial(aiTextureType_SPECULAR, scene->mMaterials[currentMaterial], sceneName);
				if (RMAO)
				{
					gr::Texture::TextureParams RMAOParams = RMAO->GetTextureParams();
					RMAOParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear;
					RMAOParams.m_texFormat = gr::Texture::TextureFormat::RGBA8;
					RMAO->SetTextureParams(RMAOParams);

					newMaterial->AccessTexture(TEXTURE_RMAO) = RMAO;
				}
				else
				{
					newMaterial->AddShaderKeyword(Shader::SHADER_KEYWORDS[NO_RMAO_TEXTURE]);
				}


				// Pack material properties:
				// Extract F0 reflectivity from "Reflected Color":
				LOG("Importing F0 value from material's \"Reflected Color\" slot");
				if (ExtractPropertyFromAiMaterial(scene->mMaterials[currentMaterial], newMaterial->Property(MATERIAL_PROPERTY_0), AI_MATKEY_COLOR_REFLECTIVE))
				{
					if (newMaterial->Property(MATERIAL_PROPERTY_0) == vec4(0))
					{
						LOG_WARNING("Found F0 value of (0,0,0). Overriding with default of (0.04, 0.04, 0.04, 0.0)");

						newMaterial->Property(MATERIAL_PROPERTY_0) = vec4(0.04f, 0.04f, 0.04f, 0.0f);
					}
					else
					{
						LOG("Inserted F0 into matProperty0 uniform: " + to_string(newMaterial->Property(MATERIAL_PROPERTY_0).x) + ", " + to_string(newMaterial->Property(MATERIAL_PROPERTY_0).y) + ", " + to_string(newMaterial->Property(MATERIAL_PROPERTY_0).z));
					}
				}
				else
				{
					#if defined(DEBUG_SCENEMANAGER_SHADER_LOGGING)
						LOG_WARNING("Could not find \"Reflected Color\" slot to extract F0 property from. Setting default of (0.04, 0.04, 0.04, 0.0)");
					#endif

					newMaterial->Property(MATERIAL_PROPERTY_0) = vec4(0.04f, 0.04f, 0.04f, 0.0f);
				}

				// Extract Phong exponent from "Cosine Power":
				LOG("Importing value from material's \"Cosine Power\" slot");
				vec4 extractedProperty(0.0f);
				if (ExtractPropertyFromAiMaterial(scene->mMaterials[currentMaterial], extractedProperty, AI_MATKEY_SHININESS))
				{
					// Need to copy the property (single channel properties are stored in .x):
					newMaterial->Property(MATERIAL_PROPERTY_0).w = extractedProperty.x;

					#if defined(DEBUG_SCENEMANAGER_SHADER_LOGGING)
						LOG("Added \"Cosine Power\" to uniform matProperty0.w: " + to_string(newMaterial->Property(MATERIAL_PROPERTY_0).x) + ", " + to_string(newMaterial->Property(MATERIAL_PROPERTY_0).y) + ", " + to_string(newMaterial->Property(MATERIAL_PROPERTY_0).z) + ", " + to_string(newMaterial->Property(MATERIAL_PROPERTY_0).w) );
					#endif
				}
				else
				{
					newMaterial->AddShaderKeyword(Shader::SHADER_KEYWORDS[NO_COSINE_POWER]);

					#if defined(DEBUG_SCENEMANAGER_SHADER_LOGGING)
						LOG_WARNING("Could not find material \"Cosine Power\" slot");
					#endif
				}

				// No need to load material shaders in deferred mode:
				if (CoreEngine::GetCoreEngine()->GetConfig()->GetValue<bool>("useForwardRendering") == true)
				{
					// Create a shader, using the keywords we've built
					bool loadedValidShader = false;
					std::size_t shaderNameIndex = matName.find_last_of("_");
					if (shaderNameIndex == string::npos)
					{
						LOG_ERROR("Could not find a shader name prefixed with an underscore in the material name. Destroying loaded textures and assigning error shader - GBuffer data will be garbage!!!");

						std::shared_ptr<Shader> newShader = Shader::CreateShader(
							CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("errorShaderName"));

						newMaterial->GetShader() = newShader;
					}
					else
					{
						string shaderName = matName.substr(shaderNameIndex + 1, matName.length() - (shaderNameIndex + 1));

						#if defined(DEBUG_SCENEMANAGER_MATERIAL_LOGGING)
							LOG("Attempting to assign shader \"" + m_shaderName + "\" to material");
						#endif

						std::shared_ptr<Shader> newShader = Shader::CreateShader(shaderName, &newMaterial->ShaderKeywords());

						if (newShader->Name() != 
							CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("errorShaderName"))
						{
							newMaterial->GetShader() = newShader;
							loadedValidShader = true;
						}
					}

					// If we didn't load a valid shader, delete any textures we might have loaded and replace them with
					// error textures:
					if (!loadedValidShader)
					{
						for (int currentTexture = 0; currentTexture < newMaterial->NumTextureSlots(); currentTexture++)
						{
							if (newMaterial->AccessTexture((TEXTURE_TYPE)currentTexture) != nullptr)
							{
								newMaterial->AccessTexture((TEXTURE_TYPE)currentTexture) = nullptr;
							}
						}

						// Assign a pink error albedo texture:
						string errorTextureName = "errorTexture"; // TODO: Store this in a config?
						newMaterial->AccessTexture(TEXTURE_ALBEDO) = 
							FindLoadTextureByPath(errorTextureName, gr::Texture::TextureColorSpace::sRGB, false);
						if (newMaterial->AccessTexture(TEXTURE_ALBEDO) == nullptr)
						{
							gr::Texture::TextureParams texParams;
							texParams.m_width = 1;
							texParams.m_height = 1;
							texParams.m_texturePath = errorTextureName;
							texParams.m_clearColor = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
							texParams.m_texturePath = errorTextureName;

							newMaterial->AccessTexture(TEXTURE_ALBEDO) = 
								std::shared_ptr<gr::Texture>(new gr::Texture(texParams));

							AddTexture(newMaterial->AccessTexture(TEXTURE_ALBEDO));
						}
					}

					// Buffer uniforms:
					newMaterial->GetShader()->UploadUniform(
						Material::MATERIAL_PROPERTY_NAMES[MATERIAL_PROPERTY_0].c_str(),
						&newMaterial->Property(MATERIAL_PROPERTY_0).x,
						UNIFORM_Vec4fv); // Upload matProperty0
				}
				

				// Add the material to our material list:
				AddMaterial(newMaterial);
			}
		}

		LOG("\nLoaded a total of " + to_string(m_textures.size()) + " textures (including error textures)\n");
	}


	void SaberEngine::SceneManager::ImportSky(string sceneName)
	{
		m_currentScene->m_skybox = std::make_shared<Skybox>(sceneName);
	}


	std::shared_ptr<gr::Texture> SaberEngine::SceneManager::ExtractLoadTextureFromAiMaterial(aiTextureType textureType, aiMaterial* material, string sceneName)
	{
		std::shared_ptr<gr::Texture> newTexture(nullptr);
		gr::Texture::TextureColorSpace colorSpace = gr::Texture::TextureColorSpace::Unknown;
		gr::Texture::TextureFormat format = gr::Texture::TextureFormat::Invalid;
	
		// Create 1x1 texture fallbacks:
		int textureCount = material->GetTextureCount(textureType);
		if (textureCount <= 0)
		{
			string newName = "NO_NAME_FOUND";
			vec4 newColor(0, 0, 0, 0);
			int texUnit = -1;

			if (textureType == aiTextureType_DIFFUSE)
			{
				// Try and find any likely texture in the material
				//newTexture = FindTextureByNameInAiMaterial("diffuse", material, sceneName); 
				// TODO: Enable this if there is a reason...

				aiColor4D color;
				if (AI_SUCCESS == material->Get("$clr.diffuse", 0, 0, color))
				{
					newName = "Color_" + to_string(color.r) + 
						"_" + to_string(color.g) + 
						"_" + to_string(color.b) + 
						"_" + to_string(color.a);
					newColor = vec4(color.r, color.g, color.b, color.a);

					texUnit = TEXTURE_0 + TEXTURE_ALBEDO;
					colorSpace = gr::Texture::TextureColorSpace::sRGB;
					format = gr::Texture::TextureFormat::RGBA8;

					LOG_WARNING("Material has no diffuse texture. Creating a 1x1 texture using the diffuse color with"
						" a path \"" + newName + "\"");
				}
			}
			else if (textureType == aiTextureType_NORMALS)
			{
				texUnit = TEXTURE_0 + TEXTURE_NORMAL;
				colorSpace = gr::Texture::TextureColorSpace::Linear;
				format = gr::Texture::TextureFormat::RGB32F;

				// Try and find any likely texture in the material
				newTexture = FindTextureByNameInAiMaterial("normal", material, sceneName);

				if (newTexture == nullptr)
				{
					// TODO: Replace this with shader multi-compiles. If no normal texture is found, use vertex normals
					// instead (for forward rendering)

					newName = "DefaultFlatNormal"; // Use a generic name, so this texture will be shared
					newColor = vec4(0.5f, 0.5f, 1.0f, 0.0f);

					LOG_WARNING("Material has no normal texture. Creating a 1x1 texture for a [0,0,1] normal with a "
						"path \"" + newName + "\"");
				}
				
			}
			else if (textureType == aiTextureType_EMISSIVE)
			{
				texUnit = TEXTURE_0 + TEXTURE_EMISSIVE;
				colorSpace = gr::Texture::TextureColorSpace::Linear; // TODO: Is emissive linear, or sRGB?
				format = gr::Texture::TextureFormat::RGBA32F; // Emissive must support values > 1

				newTexture = FindTextureByNameInAiMaterial("emissive", material, sceneName);
				if (newTexture == nullptr)
				{
					aiColor4D color;
					if (AI_SUCCESS == material->Get("$clr.emissive", 0, 0, color))
					{
						newName = "Color_" + to_string(color.r) +
							"_" + to_string(color.g) + 
							"_" + to_string(color.b) + 
							"_" + to_string(color.a);
						newColor = vec4(color.r, color.g, color.b, color.a);

						// Currently, Texture objects contain their textureUnit, so we can't share them between slots
						//newTexture = FindLoadTextureByPath(newName, false); 

						LOG_WARNING("Material has no emissive texture. Creating a 1x1 texture using the emissive "
							"(/incandesence) color property with a path \"" + newName + "\"");
					}
					else
					{
						newName = "Color_" + to_string(newColor.r) + 
							"_" + to_string(newColor.g) + 
							"_" + to_string(newColor.b) + 
							"_" + to_string(newColor.a);

						LOG_WARNING("Material has no emissive texture, and no emissive color property. Creating a 1x1 "
							"black texture with a path \"" + newName + "\"");
					}
				}
			}
			else if (textureType == aiTextureType_SPECULAR) // RGB = RMAO
			{
				texUnit = TEXTURE_0 + TEXTURE_RMAO;
				colorSpace = gr::Texture::TextureColorSpace::Linear;
				format = gr::Texture::TextureFormat::RGBA8; // ??

				const int NUM_NAMES = 3;
				string possibleNames[NUM_NAMES] = 
				{
					"roughness",
					"metallic",
					"rmao",
				};	// Add names here as necessary to increase flexibility...

				int currentName = 0;
				while (currentName < NUM_NAMES && newTexture == nullptr)
				{
					newTexture = FindTextureByNameInAiMaterial(possibleNames[currentName], material, sceneName);

					currentName++;
				}

				if (newTexture == nullptr)
				{
					// Try and use the specular color channel instead:
					aiColor4D color;
					if (AI_SUCCESS == material->Get("$clr.specular", 0, 0, color))
					{
						newName = "Color_" + to_string(color.r) + "_" + to_string(color.g) + "_" + to_string(color.b) + 
							"_" + to_string(color.a);

						newColor = vec4(color.r, color.g, color.b, color.a);

						// Currently, Texture objects contain their textureUnit, so we can't share them between slots
						//newTexture = FindLoadTextureByPath(newName, false); 

						LOG_WARNING(
							"Material has no RMAO texture in the specular slot. Creating a 1x1 texture using the "
							"specular color with a path " + newName);
					}
					else
					{
						newName = "Color_" + to_string(newColor.r) + "_" + to_string(newColor.g) + "_" + 
							to_string(newColor.b) + "_" + to_string(newColor.a);

						LOG_WARNING("Material has no RMAO texture or specular color. Creating a 1x1 black texture with "
							"a path " + newName);
					}
				}
			}
			else
			{
				LOG_WARNING("Received material does not have the requested texture. Returning nullptr!");
				return nullptr;
			}

			// Create the dummy texture:
			if (newTexture == nullptr)
			{
				// Try and find an already loaded version of our fallback texture
				newTexture = FindLoadTextureByPath(newName, colorSpace, false);

				// None exists, so create one:
				if (newTexture == nullptr)
				{
					// NOTE: Since we're storing the texUnit per-texture, we need unique textures incase they're in
					// different slots...
					gr::Texture::TextureParams texParams;
					texParams.m_width = 1;
					texParams.m_height = 1;
					texParams.m_texturePath = newName;
					texParams.m_clearColor = newColor;
					texParams.m_texColorSpace = colorSpace;
					texParams.m_texFormat = format;

					newTexture = std::shared_ptr<gr::Texture>(new gr::Texture(texParams));

					AddTexture(newTexture);	
				}
			}

			return newTexture;
		}

		if (textureCount > 1)
		{
			LOG_WARNING("Received material has " + to_string(textureCount) + " of the requested texture type... Only "
				"the first will be extracted");
		}

		string sceneRoot = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("sceneRoot") + sceneName + "\\";

		aiString path;
		material->GetTexture(textureType, 0, &path); // We only get the texture at index 0 (any others are ignored...)
		if (path.length > 0)
		{
			string texturePath = sceneRoot + string(path.C_Str());

			#if defined(DEBUG_SCENEMANAGER_TEXTURE_LOGGING)
				LOG("Found texture path: " + m_texturePath);
			#endif

			// Find the texture if it has already been loaded, or load it otherwise:
			newTexture = FindLoadTextureByPath(texturePath, gr::Texture::TextureColorSpace::Unknown);
		}
		else
		{
			LOG_ERROR("Material does not contain a diffuse texture path. Assigning an error texture");
		}

		if (newTexture == nullptr)
		{
			newTexture = FindLoadTextureByPath(INVALID_TEXTURE_PATH, gr::Texture::TextureColorSpace::Unknown);
		}

		return newTexture; // Note: Texture is currently unbuffered
	}


	std::shared_ptr<gr::Texture> SceneManager::FindTextureByNameInAiMaterial(string nameSubstring, aiMaterial* material, string sceneName)
	{
		std::transform(nameSubstring.begin(), nameSubstring.end(), nameSubstring.begin(), ::tolower);

		for (int currentTextureType = 0; currentTextureType < AI_TEXTURE_TYPE_MAX; currentTextureType++)
		{
			aiString path;
			material->GetTexture((aiTextureType)currentTextureType, 0, &path);
			if (path.length > 0)
			{
				string pathString(path.C_Str());
				std::transform(pathString.begin(), pathString.end(), pathString.begin(), ::tolower);

				if (pathString.find(nameSubstring) != string::npos)
				{
					LOG_WARNING("Texture not found in expected slot. Assigning texture containing "
						"\"" + nameSubstring + "\" as a fallback");

					string sceneRoot = 
						CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("sceneRoot") + sceneName + "\\";

					string texturePath = sceneRoot + string(path.C_Str());
					
					return FindLoadTextureByPath(texturePath, gr::Texture::TextureColorSpace::Unknown);
				}
			}
		}

		return nullptr;
	}


	bool SaberEngine::SceneManager::ExtractPropertyFromAiMaterial(
		aiMaterial* material, 
		vec4& targetProperty,
		char const* AI_MATKEY_TYPE,
		int unused0 /*= 0*/,
		int unused1 /*= 0*/) // NOTE: unused0/unused1 are required to match #defined macros
	{
		// Note: vec4 targetProperty's .a channel will always be set to 0 here.

		// Note: Single element properties (eg. Phong Cosine Power "Shininess") are inserted into the .x channel
		aiColor3D color(0.f, 0.f, 0.f); 
		if (AI_SUCCESS == material->Get(AI_MATKEY_TYPE, unused0, unused1, color))
		{
			#if defined(DEBUG_SCENEMANAGER_MATERIAL_LOGGING)
				LOG("Successfully extracted material property from AI_MATKEY_SHININESS");
			#endif

			// Note: We always initializing the last property as 0...
			targetProperty = vec4(color.r, color.g, color.b, 0.0f);	

			return true;
		}
		else
		{
			#if defined(DEBUG_SCENEMANAGER_MATERIAL_LOGGING)
				LOG_ERROR("Material property extraction failed");
			#endif

			return false;
		}
	}


	void SceneManager::ImportGameObjectGeometryFromScene(aiScene const* scene)
	{
		int numMeshes = scene->mNumMeshes;
		LOG("Found " + to_string(numMeshes) + " scene meshes");

		// Allocations:
		m_currentScene->InitMeshArray();
		
		m_currentScene->m_gameObjects.clear();
		m_currentScene->m_gameObjects.reserve(numMeshes); // Assuming that every GameObject will have at least 1 mesh...

		// Loop through each mesh in the scene graph:
		for (int currentMesh = 0; currentMesh < numMeshes; currentMesh++)
		{
			string meshName = string(scene->mMeshes[currentMesh]->mName.C_Str());

			// Check mesh is valid:
			if (
				scene->mMeshes[currentMesh]->HasPositions()
				&& scene->mMeshes[currentMesh]->HasFaces()
				&& scene->mMeshes[currentMesh]->HasNormals()
				&& scene->mMeshes[currentMesh]->HasVertexColors(0)
				&& scene->mMeshes[currentMesh]->HasTextureCoords(0)
				&& scene->mMeshes[currentMesh]->HasTangentsAndBitangents()
				)
			{
				LOG("All expected mesh properties found");
			}
			else
			{
				LOG_WARNING("Mesh \"" + meshName + "\" is missing the following properties:");
				if (!scene->mMeshes[currentMesh]->HasPositions())
					LOG_WARNING("\t - positions");

				if (!scene->mMeshes[currentMesh]->HasFaces())					
					LOG_WARNING("\t - faces");

				if (!scene->mMeshes[currentMesh]->HasNormals())					
					LOG_WARNING("\t - normals");

				if (!scene->mMeshes[currentMesh]->HasVertexColors(0))			
					LOG_WARNING("\t - vertex colors");

				if (!scene->mMeshes[currentMesh]->HasTextureCoords(0))			
					LOG_ERROR("\t - texture coordinates: The object may not render correctly!");
				if (!scene->mMeshes[currentMesh]->HasTangentsAndBitangents())	

					LOG_ERROR("\t - tangents & bitangents: The object may not render correctly!");
			}

			// Find the corresponding node in the scene graph:
			aiNode* currentNode = scene->mRootNode->FindNode(scene->mMeshes[currentMesh]->mName);
			if (currentNode)
			{
				// We've found a corresponding node in the scene graph. Create a mesh:
				int numVerts		= scene->mMeshes[currentMesh]->mNumVertices;
				int numFaces		= scene->mMeshes[currentMesh]->mNumFaces;
				int numUVs			= scene->mMeshes[currentMesh]->mNumUVComponents[0]; // Just look at the first UV channel for now...
				int numUVChannels	= scene->mMeshes[currentMesh]->GetNumUVChannels();
				int materialIndex	= scene->mMeshes[currentMesh]->mMaterialIndex;
				
				aiString name;
				scene->mMaterials[materialIndex]->Get(AI_MATKEY_NAME, name);
				string materialName = string(name.C_Str());

				#if defined(DEBUG_SCENEMANAGER_MESH_LOGGING)
					LOG("\nMesh #" + to_string(currentMesh) + " \"" + meshName + "\": " + to_string(numVerts) + 
						" verts, " + to_string(numFaces) + " faces, " + to_string(numUVChannels) + " UV channels, " + 
						to_string(numUVs) + " UV components in channel 0, using material #" + to_string(materialIndex));
				#endif

				std::vector<gr::Vertex> vertices(numVerts);

				// Add each vertex to the vertices array:
				for (int currentVert = 0; currentVert < numVerts; currentVert++)
				{
					// Default vertex values:
					vec3 position = vec3(0,0,0), normal = vec3(0, 0, 0), tangent = vec3(0, 0, 0), bitangent = vec3(0, 0, 0);
					vec4 color(0, 0, 0, 1);
					vec4 uv(0, 0, 0, 0);

					bool hasTangentsAndBitangents = false, hasNormal = false;

					// Position:
					if (scene->mMeshes[currentMesh]->HasPositions())
					{
						position = vec3(
							scene->mMeshes[currentMesh]->mVertices[currentVert].x,
							scene->mMeshes[currentMesh]->mVertices[currentVert].y, 
							scene->mMeshes[currentMesh]->mVertices[currentVert].z);
					}

					// Normal:
					if (scene->mMeshes[currentMesh]->HasNormals())
					{
						hasNormal = true;
						normal = vec3(
							scene->mMeshes[currentMesh]->mNormals[currentVert].x, 
							scene->mMeshes[currentMesh]->mNormals[currentVert].y,
							scene->mMeshes[currentMesh]->mNormals[currentVert].z);
					}

					// Vertex color:
					if (scene->mMeshes[currentMesh]->HasVertexColors(0) && scene->mMeshes[currentMesh]->mColors[0])
					{
						color = vec4(
							scene->mMeshes[currentMesh]->mColors[0][currentVert].r, 
							scene->mMeshes[currentMesh]->mColors[0][currentVert].g, 
							scene->mMeshes[currentMesh]->mColors[0][currentVert].b,
							scene->mMeshes[currentMesh]->mColors[0][currentVert].a);
					}

					// TexCoords:
					if (scene->mMeshes[currentMesh]->HasTextureCoords(0))
					{
						uv = vec4(
							scene->mMeshes[currentMesh]->mTextureCoords[0][currentVert].x, 
							scene->mMeshes[currentMesh]->mTextureCoords[0][currentVert].y, 0, 0);
					}

					// Tangents/Bitangents:
					if (scene->mMeshes[currentMesh]->HasTangentsAndBitangents())
					{
						hasTangentsAndBitangents = true;
						tangent		= vec3(
							scene->mMeshes[currentMesh]->mTangents[currentVert].x, 
							scene->mMeshes[currentMesh]->mTangents[currentVert].y,
							scene->mMeshes[currentMesh]->mTangents[currentVert].z);

						bitangent	= vec3(
							scene->mMeshes[currentMesh]->mBitangents[currentVert].x, 
							scene->mMeshes[currentMesh]->mBitangents[currentVert].y, 
							scene->mMeshes[currentMesh]->mBitangents[currentVert].z);
					}

					// Handle incorrect tangents/bitangents due to flipped UV's:
					if (hasNormal && hasTangentsAndBitangents)
					{
						if (glm::dot(glm::cross(tangent, bitangent), normal) < 0)
						{
							tangent *= -1.0f;
						}
					}

					// Assemble the vertex:
					vertices[currentVert] = gr::Vertex(position, normal, tangent, bitangent, color, uv);
				}

				// Fill the indices array:
				uint32_t numIndices = scene->mMeshes[currentMesh]->mNumFaces * 3;
				std::vector<uint32_t> indices(numIndices);

				#if defined(DEBUG_SCENEMANAGER_MESH_LOGGING)
					LOG("Created arrays of " + to_string(numVerts) + " vertices, & " + to_string(numIndices) + " indices");
				#endif					

				for (int currentFace = 0; currentFace < numFaces; currentFace++)
				{
					for (int currentIndex = 0; currentIndex < 3; currentIndex++)
					{
						if (scene->mMeshes[currentMesh]->mFaces[currentFace].mNumIndices != 3)
						{
							LOG_ERROR("Found a face that doesn't have 3 indices during mesh import!")
						}
						indices[(currentFace * 3) + currentIndex] = 
							scene->mMeshes[currentMesh]->mFaces[currentFace].mIndices[currentIndex];
					}
				}

				std::shared_ptr<gr::Mesh> newMesh = std::make_shared<gr::Mesh>(meshName, vertices, indices, GetMaterial(materialName));

				GameObject* gameObject		= FindCreateGameObjectParents(scene, currentNode->mParent);

				Transform* targetTransform	= nullptr;

				// If the mesh doesn't belong to a group, create a GameObject to contain it:
				if (gameObject == nullptr)
				{
					#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
						LOG_ERROR("Creating a GameObject for mesh \"" + meshName + "\" that did not belong to a group!"
							" GameObjects should belong to groups in the source .FBX!");
					#endif
					
					gameObject = new GameObject(meshName);
					AddGameObject(gameObject);				// Add the new game object

					// Add a postfix to remind us that we expect GameObjects to be grouped in our .FBX from Maya
					newMesh->Name() = meshName + "_MESH";	

					targetTransform = gameObject->GetTransform(); // We'll use the gameobject in our transform heirarchy
				}
				else // We have a GameObject:
				{
					#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
						LOG("Found existing parent GameObject \"" + gameObject->GetName() + "\" for mesh \"" + meshName + "\"");
					#endif

					targetTransform = &newMesh->GetTransform();	// We'll use the mesh in our transform heirarchy
				}

				// Mesh doesn't belong to a group, so we'll give it's transform to the gameobject we've created
				aiMatrix4x4 combinedTransform	= GetCombinedTransformFromHierarchy(scene, currentNode->mParent);	
				
				// Combine the parent and child transforms	
				combinedTransform				= combinedTransform * currentNode->mTransformation;					
				
				InitializeTransformValues(combinedTransform, targetTransform);		// Copy to our Mesh transform

				gameObject->GetRenderable()->AddViewMeshAsChild(newMesh);			// Creates transform heirarchy

				m_currentScene->AddMesh(newMesh);									// Also calculates scene bounds
			}
			else
			{
				LOG_ERROR("Could not find \"" + meshName + "\" in the scene graph");
			}
		}

		int numGameObjects = (int)m_currentScene->m_gameObjects.size();
		LOG("\nCreated " + to_string(numGameObjects) + " game objects");
	}


	GameObject* SaberEngine::SceneManager::FindCreateGameObjectParents(aiScene const* scene, aiNode* parent)
	{
		if (parent == nullptr || parent == scene->mRootNode)
		{
			#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
				LOG("Reached end of parent heirarchy!");
			#endif

			return nullptr;
		}

		string parentName = string(parent->mName.C_Str());

		// Exclude Maya .fbx "frozen" transformation nodes and keep searching:
		if (parentName.find("$AssimpFbx$") != string::npos)
		{	
			#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
				LOG("Found Maya transformation node \"" + parentName + "\", ignoring and continuing to search!");
			#endif
			return FindCreateGameObjectParents(scene, parent->mParent);
		}

		// Check if there is a GameObject that corresponds with the current parent node
		for (int i = 0; i < (int)m_currentScene->m_gameObjects.size(); i++)
		{
			if (m_currentScene->m_gameObjects.at(i)->GetName() == parentName)
			{
				#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
					LOG("Found an existing GameObject parent: \"" + parentName + "\"");
				#endif

				return m_currentScene->m_gameObjects.at(i);
			}
		}

		// Otherwise, create the heirarchy
		GameObject* newGameObject = new GameObject(parentName);
		InitializeTransformValues(parent->mTransformation, newGameObject->GetTransform());

		AddGameObject(newGameObject);

		GameObject* nextParent = FindCreateGameObjectParents(scene, parent->mParent);
		if (nextParent)
		{
			#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
				LOG("Parented \"" + newGameObject->GetName() + "\" -> \"" + nextParent->GetName() + "\"");
			#endif

			newGameObject->GetTransform()->Parent(nextParent->GetTransform());
		}
		
		#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
			LOG("Returning newly created GameObject \"" + newGameObject->GetName() + "\"");
		#endif

		return newGameObject;
	}


	aiMatrix4x4 SaberEngine::SceneManager::GetCombinedTransformFromHierarchy(aiScene const* scene, aiNode* parent, bool skipPostRotations /*= true*/)
	{
		if (scene == nullptr || parent == nullptr)
		{
			LOG_ERROR("SceneManager.GetCombinedTransformFromHierarchy() received a null pointer!");
			return aiMatrix4x4();
		}


		#if defined(DEBUG_SCENEMANAGER_TRANSFORM_LOGGING)
			LOG("Received parent \"" + string(m_parent->mName.C_Str()) + "\". Combining imported transformations from scene graph:");
		#endif

		#if defined(DEBUG_SCENEMANAGER_TRANSFORM_LOGGING)
			LOG("Spewing transform hierarchy:");
			aiNode* debug = m_parent;
			while (debug != nullptr)
			{
				LOG("-> " + string(debug->mName.C_Str()));

				// NOTE: Assimp matrices are stored in row major order
				LOG("\t\t" + to_string(debug->mTransformation.a1) + " " + to_string(debug->mTransformation.a2) + " " + 
					to_string(debug->mTransformation.a3) + " " + to_string(debug->mTransformation.a4));
				
				LOG("\t\t" + to_string(debug->mTransformation.b1) + " " + to_string(debug->mTransformation.b2) + " " + 
					to_string(debug->mTransformation.b3) + " " + to_string(debug->mTransformation.b4));
				
				LOG("\t\t" + to_string(debug->mTransformation.c1) + " " + to_string(debug->mTransformation.c2) + " " +
					to_string(debug->mTransformation.c3) + " " + to_string(debug->mTransformation.c4));
				
				LOG("\t\t" + to_string(debug->mTransformation.d1) + " " + to_string(debug->mTransformation.d2) + " " +
					to_string(debug->mTransformation.d3) + " " + to_string(debug->mTransformation.d4));

				debug = debug->mParent;
			}
			LOG("End of transform hierarchy!");
		#endif

		aiMatrix4x4 combinedTransform;	// Start with the identity matrix
		aiNode* current = parent;
		while (current != nullptr && current != scene->mRootNode)
		{
			string currentName = string(current->mName.C_Str());

			// HACK: Seems if we skip "_PostRotation" nodes, the directional light orientation will be correct. 
			// But, we need this for camera xforms...
			if (skipPostRotations && currentName.find("_Post") != string::npos) 
			{
				#if defined(DEBUG_SCENEMANAGER_TRANSFORM_LOGGING)
					LOG("\t\tSkipped node \"" + currentName + "\"");
				#endif

				current = current->mParent;
				continue;
			}

			if (currentName.find("$AssimpFbx$") != string::npos)
			{
				#if defined(DEBUG_SCENEMANAGER_TRANSFORM_LOGGING)
					LOG("\t\tCombined node with \"" + currentName + "\"");
				#endif	
				combinedTransform = current->mTransformation * combinedTransform;
			}
			else
			{
				#if defined(DEBUG_SCENEMANAGER_TRANSFORM_LOGGING)
					LOG("\tNode \"" + currentName + "\" is not a transform. Stopping!");
				#endif
				break;
			}

			current = current->mParent;
		}

		return combinedTransform;
	}


	aiNode* SaberEngine::SceneManager::FindNodeContainingName(aiScene const* scene, string name)
	{
		aiNode* result = nullptr;
		if (result = scene->mRootNode->FindNode(name.c_str()))
		{
			return result;
		}

		std::transform(name.begin(), name.end(), name.begin(), ::tolower);
		if (result = FindNodeRecursiveHelper(scene->mRootNode, name))
		{
			return result;
		}

		LOG("Could not find any node containing the name \"" + name + "\" in the scene graph. Returning nullptr");
		return nullptr;
	}


	aiNode* SaberEngine::SceneManager::FindNodeRecursiveHelper(aiNode* rootNode, string name)
	{
		if (rootNode == nullptr)
		{
			return nullptr;
		}

		string currentName(rootNode->mName.C_Str());
		std::transform(currentName.begin(), currentName.end(), currentName.begin(), ::tolower);
		if (currentName.find(name) != string::npos)
		{
			LOG("Found node containing \"" + name + "\", returning node \"" + currentName +"\"");
			return rootNode;
		}

		for (unsigned int i = 0; i < rootNode->mNumChildren; i++)
		{
			aiNode* result = FindNodeRecursiveHelper(rootNode->mChildren[i], name);

			if (result != nullptr)
			{
				LOG("Found child node containing \"" + name + "\", returning node \"" + string(result->mName.C_Str()) + "\"");
				return result;
			}
		}
		return nullptr;
	}


	void SaberEngine::SceneManager::ImportLightsFromScene(aiScene const* scene)
	{
		int numLights = scene->mNumLights;
		if (numLights <= 0)
		{
			LOG_ERROR("\nScene has no lights to import!");
			return;
		}
		else
		{
			LOG("\nFound " + to_string(numLights) + " scene lights");
		}		
		
		bool foundDirectional	= false;	// TEMP: Only find the first directional light
		bool foundAmbient		= false;	// Track: Have we found a point light with a name containing "ambient" yet?

		for (unsigned int i = 0; i < scene->mNumLights; i++)
		{
			switch (scene->mLights[i]->mType)
			{
			case aiLightSource_DIRECTIONAL:
			{
				if (!foundDirectional)
				{
					foundDirectional = true;
					string lightName = string(scene->mLights[i]->mName.C_Str());

					#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
						LOG("\nFound a directional light \"" + m_lightName + "\"");
					#endif

					vec3 lightColor(
						scene->mLights[i]->mColorDiffuse.r, 
						scene->mLights[i]->mColorDiffuse.g, 
						scene->mLights[i]->mColorDiffuse.b);

					std::shared_ptr<Light> keyLight = std::make_shared<Light>
					(
						lightName, 
						LIGHT_DIRECTIONAL, 
						lightColor,
						nullptr
					);

					InitializeLightTransformValues(scene, lightName, &keyLight->GetTransform());

					m_currentScene->AddLight(keyLight);

					gr::Bounds sceneWorldBounds	= m_currentScene->WorldSpaceSceneBounds();
					
					gr::Bounds transformedBounds = sceneWorldBounds.GetTransformedBounds(
						glm::inverse(m_currentScene->m_keyLight->GetTransform().Model()));

					CameraConfig shadowCamConfig;
					shadowCamConfig.m_near				= -transformedBounds.zMax();
					shadowCamConfig.m_far				= -transformedBounds.zMin();

					shadowCamConfig.m_isOrthographic	= true;
					shadowCamConfig.m_orthoLeft			= transformedBounds.xMin();
					shadowCamConfig.m_orthoRight		= transformedBounds.xMax();
					shadowCamConfig.m_orthoBottom		= transformedBounds.yMin();
					shadowCamConfig.m_orthoTop			= transformedBounds.yMax();

					// TEMP: We assume the key light will ALWAYS have a shadow
					std::shared_ptr<ShadowMap> keyLightShadowMap = std::make_shared<ShadowMap> 
					(
						lightName,
						CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowMapWidth"),
						CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowMapHeight"),
						shadowCamConfig,
						&m_currentScene->m_keyLight->GetTransform()
					);

					m_currentScene->m_keyLight->ActiveShadowMap(keyLightShadowMap);

					// Extract light metadata:
					aiNode* lightNode = scene->mRootNode->FindNode(scene->mLights[i]->mName.C_Str());
					if (lightNode)
					{
						float minShadowBias = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultMinShadowBias");
						lightNode->mMetaData->Get("minShadowBias", minShadowBias);
						keyLightShadowMap->MinShadowBias() = minShadowBias;

						float maxShadowBias = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultMaxShadowBias");
						lightNode->mMetaData->Get("maxShadowBias", maxShadowBias);
						keyLightShadowMap->MaxShadowBias() = maxShadowBias;					

						#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
							LOG("\nSetting directional light minimum shadow bias value: " + to_string(m_minShadowBias));
							LOG("\nSetting directional light maximum shadow bias value: " + to_string(m_maxShadowBias));
						#endif
					}
					else
					{
						LOG_ERROR("Could not find light node in scene hierarchy");
					}


					// Note: Assimp seems to import directional lights with their "forward" vector pointing in the opposite direction.
					// This is ok, since we use "forward" as "vector pointing towards the light" when uploading to our shaders...
					#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
						LOG("Directional light color: " + to_string(lightColor.r) + ", " + to_string(lightColor.g) + 
							", " + to_string(lightColor.b));
						LOG("Directional light position = " + 
							to_string(m_currentScene->m_keyLight.GetTransform().WorldPosition().x) + ", " +
							to_string(m_currentScene->m_keyLight.GetTransform().WorldPosition().y) + ", " + 
							to_string(m_currentScene->m_keyLight.GetTransform().WorldPosition().z));

						LOG("Directional light rotation = " +
							to_string(m_currentScene->m_keyLight.GetTransform().GetEulerRotation().x) + ", " +
							to_string(m_currentScene->m_keyLight.GetTransform().GetEulerRotation().y) + ", " + 
							to_string(m_currentScene->m_keyLight.GetTransform().GetEulerRotation().z) + " (radians)");

						LOG("Directional light rotation = " + 
							to_string(m_currentScene->m_keyLight.GetTransform().GetEulerRotation().x * (180.0f / A) ) +
							", " + to_string(m_currentScene->m_keyLight.GetTransform().GetEulerRotation().y * (180.0f / glm::pi<float>())) +
							", " + to_string(m_currentScene->m_keyLight.GetTransform().GetEulerRotation().z * (180.0f / glm::pi<float>())) + 
							" (degrees)");

						LOG("Directional light forward = " +
							to_string(m_currentScene->m_keyLight.GetTransform().Forward().x) + ", " +
							to_string(m_currentScene->m_keyLight.GetTransform().Forward().y) + ", " + 
							to_string(m_currentScene->m_keyLight.GetTransform().Forward().z));
					#endif
				}
				else
				{
					LOG_ERROR("Found additional directional light. More than 1 directional light is not yet supported!");
				}
			}
				break;

			case aiLightSource_POINT: // Can be either a point or ambient light
			{
				string lightName = string(scene->mLights[i]->mName.C_Str());

				LIGHT_TYPE pointType = LIGHT_POINT;

				// NOTE: The word "ambient" must appear in the ambient light's name
				if (!foundAmbient && lightName.find("ambient") != string::npos)	
				{
					#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
						LOG("Created ambient light from \"" + m_lightName +"\"");
					#endif

					foundAmbient = true;

					pointType = LIGHT_AMBIENT_COLOR; // Assume it's a colored ambient for now
				}
				else
				{
					#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
						LOG("\nFound a point light \"" + m_lightName + "\"");
					#endif
				}

				// == color * intensity. Both ambient and point types use the mColorDiffuse
				vec3 lightColor(
					scene->mLights[i]->mColorDiffuse.r,
					scene->mLights[i]->mColorDiffuse.g, 
					scene->mLights[i]->mColorDiffuse.b); 

				// Get ready for metadata extraction:
				float minShadowBias	= 
					CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultMinShadowBias");

				float maxShadowBias	=
					CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultMaxShadowBias");

				float shadowCamNear	= 
					CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultNear");

				int shadowCubeWidth	= 
					(int)CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowCubeMapWidth");
				int shadowCubeHeight = 
					(int)CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowCubeMapHeight");
				// TODO: These should be the same size? assert?

				// Get ready to compute point light radius, if required:
				float radius				= 1.0f;
				std::shared_ptr<ShadowMap> cubeShadowMap	= nullptr;

				// Extract metadata:
				aiNode* lightNode = scene->mRootNode->FindNode(scene->mLights[i]->mName.C_Str());
				if (pointType == LIGHT_POINT && lightNode)
				{
					// How close to zero we are: Want to maximize this, with as little visual discontinuity as possible
					float cutoff = 0.05f;	
					if (lightNode->mMetaData->Get("cutoff", cutoff))
					{
						#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
							LOG("\nImporting point light cutoff value: " + to_string(cutoff));
						#endif
					}
					
					// Want the sphere mesh radius where light intensity will be close to zero
					float maxColor = glm::max( glm::max(lightColor.r, lightColor.g), lightColor.b);			
					radius = glm::sqrt((maxColor / cutoff) - 1.0f);

					// Shadow metadata extraction:
					lightNode->mMetaData->Get("minShadowBias",	minShadowBias);
					lightNode->mMetaData->Get("maxShadowBias",	maxShadowBias);
					lightNode->mMetaData->Get("near",			shadowCamNear);

					bool gotWidth	= lightNode->mMetaData->Get("shadowWidth",	shadowCubeWidth);
					bool gotHeight	= lightNode->mMetaData->Get("shadowHeight",	shadowCubeHeight);
					if ((gotWidth || gotHeight) && shadowCubeWidth != shadowCubeHeight)
					{
						// Got width only:
						if (gotWidth && !gotHeight)
						{
							shadowCubeWidth = shadowCubeHeight;
						}
						// Got height only:
						else if (gotHeight && !gotWidth)
						{
							shadowCubeHeight = shadowCubeWidth;
						}
						// Got both: Use the smallest
						else if (gotWidth && gotHeight)
						{
							int minRes = glm::min(shadowCubeWidth, shadowCubeHeight);
							shadowCubeWidth = minRes;
							shadowCubeHeight = minRes;
						}
						LOG_WARNING("Imported mismatched shadow cube map resolutions. Assigning width = " + 
							to_string(shadowCubeWidth) + ", height = " + to_string(shadowCubeHeight));
					}
				}		

				// Create the light:
				std::shared_ptr<Light> pointLight = nullptr;
				if (pointType == LIGHT_POINT || 
					CoreEngine::GetCoreEngine()->GetConfig()->GetValue<bool>("useForwardRendering") == true)
				{
					pointLight = std::make_shared<Light>(
							lightName,
							pointType,
							lightColor,
							nullptr,
							radius); // Only used if we're actually creating a point light
				}
				else
				{
					pointLight = std::make_shared<ImageBasedLight>(
						lightName,
						CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("defaultIBLPath")); 
						// TODO: Load the HDR path from FBX (Currently not supported in Assimp???)

					// If we didn't load a valid IBL, fall back to using an ambient color light
					if (!dynamic_cast<ImageBasedLight*>(pointLight.get())->IsValid())
					{
						pointLight = std::make_shared<Light>(
							lightName,
							pointType,	// This will be AMBIENT_COLOR as set above
							lightColor,
							nullptr,
							radius); // Only used if we're actually creating a point light
					}
				}

				if (pointType == LIGHT_POINT)
				{
					// Create a cube shadow map:
					CameraConfig shadowCamConfig;
					shadowCamConfig.m_fieldOfView		= 90.0f;

					shadowCamConfig.m_near			= shadowCamNear;
					shadowCamConfig.m_far				= radius;
					
					shadowCamConfig.m_aspectRatio		= 1.0f;

					shadowCamConfig.m_isOrthographic	= false;


					// TEMP: We assume point lights ALWAYS have a shadow. TODO: Control shadow maps via .FBX metadata
					cubeShadowMap = std::make_shared<ShadowMap>(
						lightName,
						shadowCubeWidth,
						shadowCubeHeight,
						shadowCamConfig,
						&pointLight->GetTransform(),
						vec3(0.0f, 0.0f, 0.0f),		// Default value
						true);

					cubeShadowMap->MinShadowBias() = minShadowBias; // Extracted from the .FBX metadata above
					cubeShadowMap->MaxShadowBias() = maxShadowBias;		

					#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
						LOG("\nSetting directional light minimum shadow bias value: " + to_string(m_minShadowBias));
						LOG("\nSetting directional light maximum shadow bias value: " + to_string(m_maxShadowBias));
					#endif

					pointLight->ActiveShadowMap(cubeShadowMap);

					// Setup the transformation hierarchy:
					if (lightNode)
					{
						aiMatrix4x4 combinedTransform	= GetCombinedTransformFromHierarchy(scene, lightNode->mParent);
						
						// Combine the parent and child transforms	
						combinedTransform				= combinedTransform * lightNode->mTransformation; 

						GameObject* gameObject			= FindCreateGameObjectParents(scene, lightNode->mParent);

						Transform* targetTransform		= nullptr;

						// If the mesh doesn't belong to a group, create a GameObject to contain it:
						if (gameObject == nullptr)
						{
							#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
								LOG_WARNING("Creating a GameObject for light \"" + m_lightName + "\" that did not "
									"belong to a group! GameObjects should belong to groups in the source .FBX!");
							#endif
					
							gameObject = new GameObject(lightName);
							AddGameObject(gameObject);				// Add the new game object

							// We'll use the gameobject in our transform heirarchy
							targetTransform = gameObject->GetTransform(); 

							InitializeLightTransformValues(scene, lightName, &pointLight->GetTransform());
						}
						else // We have a GameObject:
						{
							#if defined(DEBUG_SCENEMANAGER_GAMEOBJECT_LOGGING)
								LOG("Found existing parent GameObject \"" + gameObject->GetName() + "\" for light \"" + 
									m_lightName + "\"");
							#endif

							// We'll use the mesh in our transform heirarchy
							targetTransform = &pointLight->GetTransform();	

							InitializeTransformValues(combinedTransform, targetTransform);
						}

						pointLight->GetTransform().Parent(gameObject->GetTransform());					
					
						#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
							LOG("Calculated point light radius of " + to_string(radius));
						#endif
					}
					
				}

				m_currentScene->AddLight(pointLight);
			}				
				break;

			case aiLightSource_SPOT:
				LOG_ERROR("Found a spot light. Spot lights are not yet supported!");
				break;

			case aiLightSource_UNDEFINED:
				LOG_ERROR("Found an undefined light type");
				break;

			default:
				LOG_ERROR("Found an unhandled light type");
				break;

			}

			// Normalize the lighting if we're in forward mode
			if (CoreEngine::GetCoreEngine()->GetConfig()->GetValue<bool>("useForwardRendering"))
			{
				vector<std::shared_ptr<Light>>const* allLights = &m_currentScene->GetDeferredLights();

				for (int i = 0; i < (int)allLights->size(); i++)
				{
					vec3 lightColor = allLights->at(i)->Color();
					float maxChannel = glm::max(glm::max(lightColor.x, lightColor.y), lightColor.z);

					if (maxChannel > 1.0f)
					{
						lightColor /= maxChannel;
						allLights->at(i)->SetColor(lightColor);
					}
				}
			}

			#if defined(DEBUG_SCENEMANAGER_LIGHT_LOGGING)
				LOG("mAngleInnerCone = " + to_string(scene->mLights[i]->mAngleInnerCone) + " radians");
				LOG("mAngleOuterCone = " + to_string(scene->mLights[i]->mAngleOuterCone) + " radians");
				LOG("mAttenuationConstant = " + to_string(scene->mLights[i]->mAttenuationConstant));
				LOG("mAttenuationLinear = " + to_string(scene->mLights[i]->mAttenuationLinear));
				LOG("mAttenuationQuadratic = " + to_string(scene->mLights[i]->mAttenuationQuadratic));
				LOG("mColorAmbient = " + to_string(scene->mLights[i]->mColorAmbient.r) + ", " +
					to_string(scene->mLights[i]->mColorAmbient.g) + ", " +
					to_string(scene->mLights[i]->mColorAmbient.b));

				LOG("mColorDiffuse = " + to_string(scene->mLights[i]->mColorDiffuse.r) + ", " +
					to_string(scene->mLights[i]->mColorDiffuse.g) + ", " + 
					to_string(scene->mLights[i]->mColorDiffuse.b));

				LOG("mColorSpecular = " + to_string(scene->mLights[i]->mColorSpecular.r) + ", " +
					to_string(scene->mLights[i]->mColorSpecular.g) + ", " + 
					to_string(scene->mLights[i]->mColorSpecular.b));

				LOG("mDirection = " + to_string(scene->mLights[i]->mDirection.x) + ", " + 
					to_string(scene->mLights[i]->mDirection.y) + ", " +
					to_string(scene->mLights[i]->mDirection.z));

				LOG("mPosition = " + to_string(scene->mLights[i]->mPosition.x) + ", " + 
					to_string(scene->mLights[i]->mPosition.y) + ", " +
					to_string(scene->mLights[i]->mPosition.z));

				LOG("mSize = " + to_string(scene->mLights[i]->mSize.x) + ", " + to_string(scene->mLights[i]->mSize.y));
				LOG("mUp = " + to_string(scene->mLights[i]->mUp.x) + ", " + to_string(scene->mLights[i]->mUp.y) + 
					", " + to_string(scene->mLights[i]->mUp.z));
			#endif
		}

		if (!foundAmbient)
		{
			LOG_WARNING("No ambient light found! An ambient light must be added to the scene for image-based lighting");
		}
	}


	void SaberEngine::SceneManager::ImportCamerasFromScene(
		aiScene const* scene /*= nullptr*/,
		bool clearCameras /*= false*/) // If scene == nullptr, create a camera at the origin
	{
		if (clearCameras)
		{
			m_currentScene->ClearCameras();
		}

		string cameraName;
		CameraConfig newCamConfig;
		std::shared_ptr<Camera> newCamera = nullptr;
		int numCameras = 0;


		if (scene == nullptr) // Signal to create a default camera at the origin
		{
			LOG("\nCreating a default camera");

			newCamConfig.m_aspectRatio	= CoreEngine::GetCoreEngine()->GetConfig()->GetWindowAspectRatio();
			newCamConfig.m_fieldOfView	= CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultFieldOfView");
			newCamConfig.m_near			= CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultNear");
			newCamConfig.m_far			= CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultFar");

			newCamConfig.m_exposure		= CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultExposure");

			cameraName					= "defaultCamera";
		}
		else
		{
			// TODO: In the current version of Assimp, camera import is broken.
			// + The mLookAt, mUp vectors seem to just be the world forward, up vectors, and mTransformation is the identity...
			// + Importing cameras facing towards Z+ results in a flipped camera?
			// + Importing cameras facing towards Z- results in an extra +90 degree rotation about Y being applied?


			// Camera configuration:
			newCamConfig.m_aspectRatio		= CoreEngine::GetCoreEngine()->GetConfig()->GetWindowAspectRatio();
			
			//scene->mCameras[0]->mHorizontalFOV; // TODO: Implement this (Needs to be converted to a vertical FOV???)
			newCamConfig.m_fieldOfView		= CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultFieldOfView"); 

			newCamConfig.m_near				= scene->mCameras[0]->mClipPlaneNear;
			newCamConfig.m_far				= scene->mCameras[0]->mClipPlaneFar;
			newCamConfig.m_isOrthographic		= false;	// This is the default, but set it here anyway for clarity

			newCamConfig.m_exposure			= CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultExposure");

			cameraName						= string(scene->mCameras[0]->mName.C_Str());

			numCameras						= scene->mNumCameras;

			// Extract metadata:
			aiNode* camNode = scene->mRootNode->FindNode(scene->mCameras[0]->mName);
			if (camNode != nullptr)
			{
				if (camNode->mMetaData->Get("exposure", newCamConfig.m_exposure))
				{
					#if defined(DEBUG_SCENEMANAGER_CAMERA_LOGGING)
						LOG("Importing camera exposure " + to_string(newCamConfig.m_exposure));
					#endif
				}
			}
		}

		if (numCameras > 1)
		{
			LOG_ERROR("\nFound " + to_string(numCameras) + " cameras in the scene. Currently, only 1 camera is "
				"supported. Setting the FIRST received camera as the main camera.");
		}
		else
		{
			LOG("Found " + to_string(numCameras) + " scene camera(s): Adding camera \"" + cameraName + "\"");
		}

		// Create a new camera, attach a GBuffer, and register:
		newCamera						= std::make_shared<Camera>(cameraName, newCamConfig);
		newCamera->AttachGBuffer();

		// For now, assume that we're only importing the main camera. No other cameras are currently supported...
		m_currentScene->RegisterCamera(CAMERA_TYPE_MAIN, newCamera);

		// Copy transform values:
		if (scene != nullptr)
		{
			
			aiNode* camNode = scene->mRootNode->FindNode(scene->mCameras[0]->mName);
			if (camNode)
			{			
				#if defined(DEBUG_SCENEMANAGER_CAMERA_LOGGING) || defined(DEBUG_TRANSFORMS)
					LOG("-> " + cameraName + " (Camera's first transformation node)");
					
					LOG(to_string(camNode->mTransformation.a1) + " " + to_string(camNode->mTransformation.a2) + " " + 
						to_string(camNode->mTransformation.a3) + " " + to_string(camNode->mTransformation.a4));
					
					LOG(to_string(camNode->mTransformation.b1) + " " + to_string(camNode->mTransformation.b2) + " " + 
						to_string(camNode->mTransformation.b3) + " " + to_string(camNode->mTransformation.b4));
					
					LOG(to_string(camNode->mTransformation.c1) + " " + to_string(camNode->mTransformation.c2) + " " + 
						to_string(camNode->mTransformation.c3) + " " + to_string(camNode->mTransformation.c4));
					
					LOG(to_string(camNode->mTransformation.d1) + " " + to_string(camNode->mTransformation.d2) + " " + 
						to_string(camNode->mTransformation.d3) + " " + to_string(camNode->mTransformation.d4));
				#endif

				aiMatrix4x4 camTransform	= GetCombinedTransformFromHierarchy(scene, camNode->mParent, false);
				camTransform				= camTransform * camNode->mTransformation;

				InitializeTransformValues(camTransform, newCamera->GetTransform());
			}

			LOG_ERROR("Camera field of view is NOT currently loaded from the source file. A hard-coded default value is"
				" used for now");
		}		

		#if defined(DEBUG_SCENEMANAGER_CAMERA_LOGGING)
			
			vec3 camRotation = newCamera->GetTransform()->GetEulerRotation();

			LOG("Camera is located at " + to_string(camPosition.x) + " " + to_string(camPosition.y) + " " +
				to_string(camPosition.z) + ". Near = " + to_string(scene->mCameras[0]->mClipPlaneNear) + ", " + 
				"far = " + to_string(scene->mCameras[0]->mClipPlaneFar) );
			
			LOG("Camera rotation is " + to_string(camRotation.x) + " " + to_string(camRotation.y) + " " + 
				to_string(camRotation.z) + " (radians)");
			
			LOG("Camera rotation is " + to_string(camRotation.x * (180.0f / glm::pi<float>())) + " " + 
				to_string(camRotation.y * (180.0f / glm::pi<float>())) + " " +
				to_string(camRotation.z * (180.0f / glm::pi<float>())) + " (degrees)");
		#endif		
	}
}


