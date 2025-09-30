#version 330 core

out vec4 FragColor;

in VS_OUT {
	vec3 FragPos;
	vec3 Normal;
	vec2 TexCoords;
	float ExplosionFactor;
} fs_in;

uniform vec3 viewPos;
uniform float ambientStrength;
uniform float materialShininess;
uniform float time;

struct DirLight {
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

struct PointLight {
	vec3 position;

	float constant;
	float linear;
	float quadratic;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

struct SpotLight {
	vec3 position;
	vec3 direction;
	float cutOff;
	float outerCutOff;

	float constant;
	float linear;
	float quadratic;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

uniform DirLight dirLight;
uniform PointLight pointLights[4];
uniform SpotLight spotLight;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_diffuse2;
uniform sampler2D texture_diffuse3;
uniform sampler2D texture_diffuse4;
uniform sampler2D texture_specular1;
uniform sampler2D texture_specular2;
uniform sampler2D texture_specular3;
uniform sampler2D texture_emissive1;

vec3 sampleDiffuse(vec2 uv)
{
	vec3 color = vec3(0.0);
	float weight = 0.0;

	vec4 tex1 = texture(texture_diffuse1, uv);
	color += tex1.rgb;
	weight += tex1.a;

	vec4 tex2 = texture(texture_diffuse2, uv);
	color += tex2.rgb;
	weight += tex2.a;

	vec4 tex3 = texture(texture_diffuse3, uv);
	color += tex3.rgb;
	weight += tex3.a;

	vec4 tex4 = texture(texture_diffuse4, uv);
	color += tex4.rgb;
	weight += tex4.a;

	if (weight > 0.0)
		color /= weight;

	if (max(max(color.r, color.g), color.b) < 0.001)
		color = vec3(0.55);

	return color;
}

vec3 sampleSpecular(vec2 uv)
{
	vec3 color = vec3(0.0);
	float weight = 0.0;

	vec4 tex1 = texture(texture_specular1, uv);
	color += tex1.rgb;
	weight += tex1.a;

	vec4 tex2 = texture(texture_specular2, uv);
	color += tex2.rgb;
	weight += tex2.a;

	vec4 tex3 = texture(texture_specular3, uv);
	color += tex3.rgb;
	weight += tex3.a;

	if (weight > 0.0)
		color /= weight;

	if (max(max(color.r, color.g), color.b) < 0.001)
		color = vec3(0.25);

	return color;
}

vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specColor)
{
	vec3 lightDir = normalize(-light.direction);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialShininess);

	vec3 ambient = ambientStrength * light.ambient * diffuseColor;
	vec3 diffuse = light.diffuse * diff * diffuseColor;
	vec3 specular = light.specular * spec * specColor;

	return ambient + diffuse + specular;
}

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specColor)
{
	vec3 lightDir = normalize(light.position - fragPos);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialShininess);

	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

	vec3 ambient = ambientStrength * light.ambient * diffuseColor;
	vec3 diffuse = light.diffuse * diff * diffuseColor;
	vec3 specular = light.specular * spec * specColor;

	return (ambient + diffuse + specular) * attenuation;
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specColor)
{
	vec3 lightDir = normalize(light.position - fragPos);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialShininess);

	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

	float theta = dot(lightDir, normalize(-light.direction));
	float epsilon = light.cutOff - light.outerCutOff;
	float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

	vec3 ambient = ambientStrength * light.ambient * diffuseColor;
	vec3 diffuse = light.diffuse * diff * diffuseColor;
	vec3 specular = light.specular * spec * specColor;

	return (ambient + diffuse + specular) * attenuation * intensity;
}

void main()
{
	vec3 normal = normalize(fs_in.Normal);
	vec3 viewDir = normalize(viewPos - fs_in.FragPos);

	vec3 diffuseColor = sampleDiffuse(fs_in.TexCoords);
	vec3 specularColor = sampleSpecular(fs_in.TexCoords);

	// Mild iridescent sheen that grows during explosion
	float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), 3.0);
	vec3 burstTint = mix(vec3(0.08, 0.12, 0.35), vec3(0.45, 0.15, 0.35), fs_in.ExplosionFactor);
	vec3 sheen = burstTint * fresnel * (0.3 + fs_in.ExplosionFactor * 0.7);

	vec3 result = vec3(0.0);
	result += calcDirLight(dirLight, normal, viewDir, diffuseColor, specularColor);
	for (int i = 0; i < 4; ++i)
		result += calcPointLight(pointLights[i], normal, fs_in.FragPos, viewDir, diffuseColor, specularColor);
	result += calcSpotLight(spotLight, normal, fs_in.FragPos, viewDir, diffuseColor, specularColor);

	vec3 emissive = texture(texture_emissive1, fs_in.TexCoords).rgb;
	if (max(max(emissive.r, emissive.g), emissive.b) > 0.01)
		result += emissive;

	result += sheen;

	FragColor = vec4(result, 1.0);
}
