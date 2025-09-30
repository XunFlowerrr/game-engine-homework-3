#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	float ExplosionFactor;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float time;
uniform float disassembleDistance;

uniform vec3 meshCenter;
uniform vec3 meshDirection;
uniform vec3 meshRotationAxis;
uniform float meshRotationAmount;
uniform float meshPhaseOffset;
uniform float meshTravelScale;

mat3 rotationMatrix(vec3 axis, float angle)
{
	axis = normalize(axis);
	float s = sin(angle);
	float c = cos(angle);
	float oc = 1.0 - c;

	return mat3(
		oc * axis.x * axis.x + c,             oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
		oc * axis.x * axis.y + axis.z * s,    oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
		oc * axis.z * axis.x - axis.y * s,    oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c
	);
}

float smoothCycle(float t)
{
	return t * t * (3.0 - 2.0 * t);
}

void main()
{
	float oscillation = sin(time + meshPhaseOffset) * 0.5 + 0.5;
	float phase = smoothCycle(clamp(oscillation, 0.0, 1.0));
	vs_out.ExplosionFactor = phase;

	float angle = meshRotationAmount * phase;
	mat3 rotation = rotationMatrix(meshRotationAxis, angle);

	vec3 translatedCenter = meshCenter + meshDirection * (disassembleDistance * meshTravelScale * phase);
	vec3 localOffset = aPos - meshCenter;
	vec3 rotatedPosition = rotation * localOffset;
	vec3 animatedPosition = translatedCenter + rotatedPosition;

	vec4 worldPosition = model * vec4(animatedPosition, 1.0);
	vs_out.FragPos = worldPosition.xyz;

	mat3 normalMatrix = mat3(transpose(inverse(model)));
	vec3 rotatedNormal = rotation * aNormal;
	vs_out.Normal = normalize(normalMatrix * rotatedNormal);
	vs_out.TexCoords = aTexCoords;

	gl_Position = projection * view * worldPosition;
}
