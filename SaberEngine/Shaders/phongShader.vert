#version 430 core
#define SABER_VERTEX_SHADER
#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


// Phong vertex shader
void main()
{
	// Assign position to the predefined gl_Position clip-space output:
    gl_Position = in_mvp * vec4(in_position.x, in_position.y, in_position.z, 1.0);

	data.vertexColor = in_color * vec4(ambientColor, 1);

	data.vertexWorldNormal = (in_model * vec4(in_normal, 0.0f)).xyz;	// Object -> World vertex normal

	data.uv0		= in_uv0;
	data.viewPos	= -(in_mv * vec4(in_position.xyz, 1.0f)).xyz;	// Negate, because camera is looking down Z-
	data.worldPos	= (in_model * vec4(in_position.xyz, 1.0f)).xyz;
	data.shadowPos	= (shadowCam_vp * vec4(data.worldPos, 1)).xyz;
	data.TBN		= AssembleTBN(in_tangent, in_bitangent, in_modelRotation);
}