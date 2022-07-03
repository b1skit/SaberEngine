// Saber Engine Error Shader.
// If this file cannot be found or cannot be compiled, the engine will likely crash.

#version 430 core

// Set the location of the position input variable
layout (location = 0) in vec3 in_position;

uniform mat4 in_mvp;

void main()
{
	// Assign our position data to the predefined gl_Position output
    gl_Position = in_mvp * vec4(in_position.x, in_position.y, in_position.z, 1.0);
}