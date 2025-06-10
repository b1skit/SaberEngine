// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "TextureTarget.h"
#include "TextureTarget_Platform.h"


namespace opengl
{
	class TextureTarget
	{
	public:
		struct PlatObj final : public re::TextureTarget::PlatObj
		{
			//
		};
	};


	class TextureTargetSet
	{
	public:
		struct PlatObj final : public re::TextureTargetSet::PlatObj
		{
			PlatObj();
			~PlatObj() override;

			GLuint m_frameBufferObject;			
		};

		static void CreateColorTargets(re::TextureTargetSet const&);
		static void AttachColorTargets(re::TextureTargetSet const&);

		static void CreateDepthStencilTarget(re::TextureTargetSet const&);
		static void AttachDepthStencilTarget(re::TextureTargetSet const&);

		static void ClearColorTargets(
			bool const* colorClearModes,
			glm::vec4 const* colorClearVals,
			uint8_t numColorClears, 
			re::TextureTargetSet const&);

		static void ClearTargets(
			bool const* colorClearModes,
			glm::vec4 const* colorClearVals,
			uint8_t numColorClears,
			bool depthClearMode,
			float depthClearVal,
			bool stencilClearMode,
			uint8_t stencilClearVal,
			re::TextureTargetSet const&);

		static void ClearDepthStencilTarget(
			bool depthClearMode,
			float depthClearVal,
			bool stencilClearMode,
			uint8_t stencilClearVal,
			re::TextureTargetSet const&);

		// TODO: Find a more suitable location than TextureTargetSet to own UAV clears
		static void ClearImageTextures(std::vector<re::RWTextureInput> const&, glm::vec4 const& clearVal);
		static void ClearImageTextures(std::vector<re::RWTextureInput> const&, glm::uvec4 const& clearVal);

		static void AttachTargetsAsImageTextures(re::TextureTargetSet const&); // ~Compute target/UAV

		static void CopyTexture(core::InvPtr<re::Texture> const& src, core::InvPtr<re::Texture> const& dst);
	};
}