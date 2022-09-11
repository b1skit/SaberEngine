#pragma once

#include <string>

#include "Light.h"	// Base class
#include "TextureTarget.h"



namespace gr
{
	class ImageBasedLight : public virtual gr::Light
	{
	public:
		enum IBLType
		{
			IEM,	// Irradience Environment Map
			PMREM,	// Pre-filtered Mipmapped Radience Environment Map

			RawHDR,	// Unfiltered: Used for straight conversion of equirectangular map to cubemap

			IBL_Count
		};

	public:
		ImageBasedLight(std::string const& lightName, std::string const& relativeHDRPath);
		~ImageBasedLight();

		// Only 1 image-based light per scene; no need to copy/duplicate
		ImageBasedLight(ImageBasedLight const&) = delete;
		ImageBasedLight(ImageBasedLight&&) = delete;
		ImageBasedLight& operator=(ImageBasedLight const&) = delete;
		ImageBasedLight() = delete;
		

		std::shared_ptr<gr::Texture> GetIEMTexture() { return m_IEM_Tex; }
		std::shared_ptr<gr::Texture> GetPMREMTexture() { return m_PMREM_Tex; }		
		std::shared_ptr<gr::Texture> GetBRDFIntegrationMap() { return m_BRDF_integrationMap; }

		// Check if an IBL was successfully loaded
		bool IsValid() const { return m_IEM_Tex != nullptr && m_PMREM_Tex != nullptr; }


		// Public static functions:
		//-------------------------

		// Convert an equirectangular HDR image to a cubemap
		// hdrPath is relative to the scene path, with no leading slash eg. "IBL\\ibl.hdr"
		// iblType controls the filtering (IEM/PMREM/None) applied to the converted cubemap 
		static std::shared_ptr<gr::Texture> ConvertEquirectangularToCubemap(
			std::string sceneName, 
			std::string relativeHDRPath, 
			int xRes, 
			int yRes, 
			IBLType iblType = RawHDR);
		// ^^^^^^^^^^^^Used to generate skybox AND ambient lighting...

	private:
		std::shared_ptr<gr::Texture> m_IEM_Tex;	// Irradiance Environment Map (IEM)
		std::shared_ptr<gr::Texture> m_PMREM_Tex; // Pre-filtered Mip-mapped Radiance Environment Map (PMREM)
		uint32_t m_maxMipLevel; // Highest valid mip level for the PMREM cube map

		// Generated BRDF integration map, required for PMREM calculations
		std::shared_ptr<gr::Texture> m_BRDF_integrationMap;
		gr::TextureTargetSet m_BRDF_integrationMapStageTargetSet;

		// Cubemap face/single texture resolution:
		int m_xRes;
		int m_yRes;


		// Private helper functions:
		//--------------------------
		void GenerateBRDFIntegrationMap(); // Generate our pre-integrated BRDF response LUT

		// TODO: Refactor BRDF pre-integration to a single-frame GS that runs once at the beginning of DeferredLighting GS



	};
}


