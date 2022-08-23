#pragma once

#include <string>

#include "Light.h"	// Base class
#include "TextureTarget.h"



namespace SaberEngine
{
	enum IBL_TYPE
	{
		IBL_IEM,		// Irradience Environment Map
		IBL_PMREM,		// Pre-filtered Mipmapped Radience Environment Map

		RAW_HDR,		// Unfiltered: Used for straight conversion of equirectangular map to cubemap

		IBL_COUNT		// RESERVED: The number of IBL texture types supported
	};


	class ImageBasedLight : public Light
	{
	public:
		ImageBasedLight(std::string lightName, std::string relativeHDRPath);

		~ImageBasedLight();

		std::shared_ptr<gr::Texture> GetIEMTexture() { return m_IEM_Tex; }
		std::shared_ptr<gr::Texture> GetPMREMTexture() { return m_PMREM_Tex; }		
		std::shared_ptr<gr::Texture> GetBRDFIntegrationMap() { return m_BRDF_integrationMap; }

		// Check if an IBL was successfully loaded
		bool IsValid() const { return m_IEM_isValid && m_PMREM_isValid; }


		// Public static functions:
		//-------------------------

		// Convert an equirectangular HDR image to a cubemap
		// hdrPath is relative to the scene path, with no leading slash eg. "IBL\\ibl.hdr"
		// iblType controls the filtering (IEM/PMREM/None) applied to the converted cubemap 
		// Returns an array of 6 textures
		static std::shared_ptr<gr::Texture> ConvertEquirectangularToCubemap(
			std::string sceneName, 
			std::string relativeHDRPath, 
			int xRes, 
			int yRes, 
			IBL_TYPE iblType = RAW_HDR);

	private:

		// TODO: MOVE INITIALIZATION TO CTOR INIT LIST

		std::shared_ptr<gr::Texture> m_IEM_Tex = nullptr;	// Irradiance Environment Map (IEM)
		std::shared_ptr<gr::Texture> m_PMREM_Tex = nullptr; // Pre-filtered Mip-mapped Radiance Environment Map (PMREM)

		uint32_t m_maxMipLevel = -1;		// Highest valid mip level for the PMREM cube map

		// Generated BRDF integration map, required for PMREM calculations
		std::shared_ptr<gr::Texture> m_BRDF_integrationMap	= nullptr;
		gr::TextureTargetSet m_BRDF_integrationMapStageTargetSet;

		// Cubemap face/single texture resolution:
		int m_xRes							= 512;
		int m_yRes							= 512;

		bool m_IEM_isValid		= false; // Is the IEM valid? (Ie. Were IBL textures successfully loaded?)
		bool m_PMREM_isValid	= false; // Is the PMREM valid? (Ie. Were IBL textures successfully loaded?)


		// Private helper functions:
		//--------------------------
		void GenerateBRDFIntegrationMap(); // Generate our pre-integrated BRDF response LUT
	};
}


