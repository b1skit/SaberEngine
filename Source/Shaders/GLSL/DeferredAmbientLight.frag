#define SABER_VEC4_OUTPUT

#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{	
	const vec2 screenUV = vOut.uv0.xy; // Ambient is drawn with a fullscreen quad

	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	// Reconstruct the world position:
	const vec4 worldPos = vec4(GetWorldPos(screenUV, gbuffer.NonLinearDepth, g_invViewProjection), 1.f);

	const vec4 viewPosition = g_view * worldPos;
	const vec3 viewEyeDir = normalize(-viewPosition.xyz);	// View-space eye/camera direction

	const mat3 viewRotationScale = mat3(g_view); // Ignore the translation
	const vec3 viewNormal = normalize(viewRotationScale * gbuffer.WorldNormal); // View-space surface MatNormal

	const float NoV = max(0.0, dot(viewNormal, viewEyeDir) );

	vec3 F0	= gbuffer.MatProp0.rgb; // .rgb = F0 (Surface response at 0 degrees)
	F0 = mix(F0, gbuffer.LinearAlbedo, gbuffer.Metalness); 
	// Linear interpolation: x, y, using t=[0,1]. Returns x when t=0 -> Blends towards MatAlbedo for metals

//	vec3 fresnel_kS = FresnelSchlick(NoV, F0); // Doesn't quite look right: Use FresnelSchlick_Roughness() instead
	const vec3 fresnel_kS = FresnelSchlick_Roughness(NoV, F0, gbuffer.Roughness);
	const vec3 k_d = 1.0 - fresnel_kS;	

	// Sample the diffuse irradiance from our prefiltered irradiance environment map.
	// Note: We must flip the Y component of our normal to compensate for our UV (0,0) top-left convention
	const vec3 diffuseCubeSampleDir = vec3(gbuffer.WorldNormal.x, -gbuffer.WorldNormal.y, gbuffer.WorldNormal.z);
	const vec3 irradiance = texture(CubeMap0, diffuseCubeSampleDir).xyz * gbuffer.AO;

	// Get the specular reflectance term:
	const vec3 worldView = normalize(g_cameraWPos - worldPos.xyz); // Direction = Point -> Eye
	vec3 worldReflection = normalize(reflect(-worldView, gbuffer.WorldNormal));
	worldReflection.y *= -1; // Note: We flip Y here to compensate for our UV (0,0) top-left convention

	const float roughness = gbuffer.Roughness;
	const float remappedRoughness = RemapRoughnessIBL(roughness);

	// Sample our generated BRDF Integration map using the non-remapped roughness
	const vec2 BRDF = texture(Tex7, vec2(max(NoV, 0.0), roughness) ).rg; 

	const vec3 specular = 
		textureLod(CubeMap1, worldReflection, remappedRoughness * g_maxPMREMMip).xyz * ((fresnel_kS * BRDF.x) + BRDF.y);

	const vec3 combinedContribution = (gbuffer.LinearAlbedo * irradiance * k_d + specular); // Note: Omitted the "/ PI" factor here

	// Apply exposure:
	const vec3 exposedColor = ApplyExposure(combinedContribution, g_exposureProperties.x);

	FragColor = vec4(exposedColor, 1.0); // Note: Omitted the "/ PI" factor here
}