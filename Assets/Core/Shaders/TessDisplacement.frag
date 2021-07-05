/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#version 440 core
#pragma import_defines ( ENABLE_CLOUDS )
#pragma import_defines ( ENABLE_SHADOWS )
#pragma import_defines ( ENABLE_DETAIL_ALBEDO_TEXTURES )

#include "CloudShadows.h"
#include "DepthPrecision.h"
#include "NormalMapping.h"
#include "Ocean.h"
#include "Rerange.h"
#include "Saturate.h"
#include "Brdfs/BlinnPhong.h"
#include "Brdfs/Lambert.h"
#include "Noise/Fbm.h"
#include "Shadows/Shadows.h"

in vec3 texCoord;
in vec3 position_worldSpace;
in vec3 wrappedNoiseCoord;
in float cameraDistance;
in float elevation;
in vec3 sunIrradiance;
in vec3 skyIrradiance;
in vec3 transmittance;
in vec3 skyRadianceToPoint;
in float logZ;

out vec4 color;

uniform sampler2D normalSampler;
uniform sampler2D attributeSampler;
uniform sampler2D albedoDetailSamplers[10];
uniform sampler2D forestAlbedoSampler;
uniform sampler2D overallAlbedoSampler;
uniform sampler2D cloudSampler;

uniform vec2 heightMapUvScale;
uniform vec2 heightMapUvOffset;
uniform vec2 overallAlbedoMapUvScale;
uniform vec2 overallAlbedoMapUvOffset;
uniform vec2 attributeMapUvScale;
uniform vec2 attributeMapUvOffset;
uniform vec3 lightDirection;
uniform vec3 cameraPosition;
uniform vec3 ambientLightColor;

uniform float forestAlbedoTransitionRange;
const float oneOnForestAlbedoBlendDistance = 1.0f / 1000.0f;
const float barrenCoverageAmounts[11] = float[11](1,0,0,0, 0,0,0,0, 0,0,0);
const float forestCoverageAmounts[11] = float[11](0,0,0,0, 0,0,0,0, 0,1,0);
const float sandDepositableAmounts[11] = float[11](1,0,0,0, 0,1,1,1, 1,1,0);
const float snowCoverageAmounts[11] = float[11](0,0,0,0, 0,0,0,0, 0,0,1);

const float detailAlbedoUvScale[11] = float[11](0.01,0.01,0.01, 0.001,0.001,0.01, 0.01,0.001,0.001, 0.001,0.01);

const float vegetationSlopeSinTheta = 0.7f;
const float oneOnVegetationSlopeBlend = 100.0;

float snowShininess = 16;


vec4 textureTriplanar(sampler2D sampler, vec3 position, vec3 normal)
{
	float tighten = 0.4679f; 
	vec3 weights = clamp(abs(normal) - tighten, 0, 1);
	
	float total = weights.x + weights.y + weights.z;
	weights /= total;
	
	vec4 cXY = texture(sampler, position.xy);
	vec4 cXZ = texture(sampler, position.xz);
	vec4 cYZ = texture(sampler, position.yz);	
	
	return weights.z * cXY + weights.y * cXZ + weights.x * cYZ; 
}

ivec4 bilinearFetchIndices(sampler2D tex, vec2 uv, out vec4 weights)
{// TODO: consider clamping at edges here
	ivec2 dims = textureSize(tex, 0);
	vec2 coordFloat = uv * vec2(dims) - 0.5f;
	ivec2 coordInt = ivec2(floor(coordFloat));
	vec2 weight = coordFloat - coordInt;
	coordInt = clamp(coordInt, ivec2(0), dims-ivec2(2));
	
	float c00 = texelFetch(tex, coordInt, 0).a;
	float c10 = texelFetch(tex, coordInt + ivec2(1, 0), 0).a;
	float c01 = texelFetch(tex, coordInt + ivec2(0, 1), 0).a;
	float c11 = texelFetch(tex, coordInt + ivec2(1, 1), 0).a;

	weights.x = (1-weight.x) * (1-weight.y);
	weights.y = weight.x * (1-weight.y);
	weights.z = (1-weight.x) * weight.y;
	weights.w = weight.x * weight.y;
	
	return ivec4(c00*255.f + 0.5f, c10*255.f + 0.5f, c01*255.f + 0.5f, c11*255.f + 0.5f);
}

ivec4 bilinearFetchIndicesWithNoise(sampler2D tex, vec2 uv, out vec4 weights, vec2 noise)
{// TODO: consider clamping at edges here
	ivec2 dims = textureSize(tex, 0);
	vec2 noiseOffset = 3 * (noise-vec2(0.5));
	vec2 coordFloat = uv * vec2(dims) - 0.5f + noiseOffset;
	ivec2 coordInt = ivec2(floor(coordFloat));
	vec2 weight = coordFloat - coordInt;
	coordInt = clamp(coordInt, ivec2(0), dims-ivec2(2));
	
	float c00 = texelFetch(tex, coordInt, 0).r;
	float c10 = texelFetch(tex, coordInt + ivec2(1, 0), 0).r;
	float c01 = texelFetch(tex, coordInt + ivec2(0, 1), 0).r;
	float c11 = texelFetch(tex, coordInt + ivec2(1, 1), 0).r;

	weights.x = (1-weight.x) * (1-weight.y);
	weights.y = weight.x * (1-weight.y);
	weights.z = (1-weight.x) * weight.y;
	weights.w = weight.x * weight.y;
	
	return ivec4(c00*255.f + 0.5f, c10*255.f + 0.5f, c01*255.f + 0.5f, c11*255.f + 0.5f);
}

ivec4 fetchIndicesWithNoise(sampler2D tex, vec2 uv, out vec4 weights, vec2 noise)
{// TODO: consider clamping at edges here
	vec2 dims = textureSize(tex, 0);
	vec2 coord = uv + 3 * (noise-vec2(0.5)) / dims;
	ivec2 coordInt = ivec2(coord * dims);

	float c = texelFetch(tex, coordInt, 0).r;

	weights = vec4(1,0,0,0);	
	return ivec4(c*255.f + 0.5f, 0, 0, 0);
}

float calcSlopeConvergence(vec2 uv)
{
	vec2 offset = 1.0 / textureSize(normalSampler, 0);
	vec2 x0 = getTerrainNormalFromRG(texture(normalSampler, uv + vec2(-offset.x, 0))).xz;
	vec2 x1 = getTerrainNormalFromRG(texture(normalSampler, uv + vec2( offset.x, 0))).xz;
	vec2 y0 = getTerrainNormalFromRG(texture(normalSampler, uv + vec2(0, -offset.y))).xz;
	vec2 y1 = getTerrainNormalFromRG(texture(normalSampler, uv + vec2(0,  offset.y))).xz;
	
	return (x0.x - x1.x) + (y0.y - y1.y);
}

vec4 blend(vec4 texture1, float a1, vec4 texture2, float a2)  
{  
    float depth = 0.2;  
    float ma = max(texture1.a + a1, texture2.a + a2) - depth;  
  
    float b1 = max(texture1.a + a1 - ma, 0);  
    float b2 = max(texture2.a + a2 - ma, 0);  
  
    return (texture1 * b1 + texture2 * b2) / (b1 + b2);  
}

vec4 sampleDetailAlbedo(int i, vec3 normal, vec3 texCoord, vec2 normalUv)
{
	return texture(albedoDetailSamplers[i], 10*texCoord.xy);
}

vec4 sampleDetailAlbedoAdvanced(int i, vec3 normal, vec3 texCoord, vec2 normalUv)
{

	if (i == 0)	// barren
	{
		color = textureTriplanar(albedoDetailSamplers[1], texCoord, normal);
		
		float slope = 1 + normal.z;
		float slopeNoise = scaledFbm(wrappedNoiseCoord, 100) * 0.2;
		float slopeWeight = saturate(rerange(slope + slopeNoise, 0.15, 0.5));
		
		vec3 rockTextureNormal = normalize(vec3(normal.x, normal.y, 0));
		vec4 rockColor = textureTriplanar(albedoDetailSamplers[0], texCoord * 0.2, rockTextureNormal);
		color = blend(color, 1-slopeWeight, rockColor, slopeWeight);
		//color = mix(color, rockColor, slopeWeight);
		
		float deposition = calcSlopeConvergence(normalUv);
		float depositionNoise = scaledFbm(wrappedNoiseCoord, 200) * 0.2;
		float depositionWeight = saturate(rerange(deposition + depositionNoise, 0, 1));
		
		vec4 depositionColor = textureTriplanar(albedoDetailSamplers[2], texCoord, normal);
		color = blend(color, 1-depositionWeight, depositionColor, depositionWeight);
		//color = mix(color, depositionColor, depositionWeight);
		color.a = 1;
		
	}
	else if (i < 10)
	{
		color = texture(albedoDetailSamplers[i], texCoord.xy);
		color.r += 0.05 * scaledFbm(wrappedNoiseCoord, 10);
		color.g += 0.05 * scaledFbm(wrappedNoiseCoord, 80);
	}
	else
	{
		color = vec4(1); // snow
	}
	
	return color;
}

vec3 fbmNormal()
{
	vec3 period = vec3(10);
	vec3 p = wrappedNoiseCoord * period;
	
	float step = 0.1;
	float scale = 1 * step;
	float h00 = fbm(p, period);
	float h10 = fbm(p + vec3(step,0,0), period);
	float h01 = fbm(p + vec3(0,step,0), period);
	vec3 tangent = vec3(step,0,scale*(h10 - h00));
	vec3 bitangent = vec3(0,step,scale*(h01 - h00));
	return normalize(cross(bitangent, tangent));
}

mat3 toTbnMatrix(vec3 normal)
{
	vec3 tangent = normalize(cross(-normal, vec3(0,1,0)));
	vec3 bitangent = cross(tangent, -normal);
	return transpose(mat3(tangent, bitangent, -normal));
}

vec4 sampleAttributeDetailTextures(vec3 normal, vec2 normalUv)
{
	vec3 detailTexCoordPerMeter = wrappedNoiseCoord * 10000.0; // repeats [0,1) per meter
	vec3 defaultAlbedoDetailTexCoord = detailAlbedoUvScale[0] * detailTexCoordPerMeter;
	vec2 attributeTexCoord = texCoord.xy * attributeMapUvScale + attributeMapUvOffset;
	
	// Sample and blend attribute albedos
	vec4 attrWeights;
//#define TERRAIN_DETAIL_NOISE_BLEND
#ifdef TERRAIN_DETAIL_NOISE_BLEND
	vec2 noise = texture(noiseSampler, attributeTexCoord * 5).rg;
	ivec4 attrIndices = bilinearFetchIndicesWithNoise(attributeSampler, attributeTexCoord, attrWeights, noise);
	//ivec4 attrIndices = fetchIndicesWithNoise(attributeSampler, attributeTexCoord, attrWeights, noise);
#else
	ivec4 attrIndices = bilinearFetchIndices(attributeSampler, attributeTexCoord, attrWeights);
#endif

#define DETAIL_HEIGHTMAP_BLEND
#ifdef DETAIL_HEIGHTMAP_BLEND
	// Use the detail maps' alpha channel as a heightmap to do height-aware blending between materials.
	// Texels with a higher height will show on top, e.g grass should appear over dirt.
	vec4 attributeColors[4];
	float maxHeight = 0.0;
	for (int i = 0; i < 4; ++i)
	{
		int index = attrIndices[i];

		vec3 albedoDetailTexCoord = detailAlbedoUvScale[index] * detailTexCoordPerMeter;
		attributeColors[i] = sampleDetailAlbedo(index, normal, albedoDetailTexCoord, normalUv);
		attributeColors[i].a *= attrWeights[i];
		maxHeight = max(maxHeight, attributeColors[i].a);
	}
	
	maxHeight -= 0.1; // controls the amount of feather on the blend
	
	vec4 attributeColor = vec4(0);
	float totalWeight = 0;
	for (int i = 0; i < 4; ++i)
	{
		float weight = max(attrWeights[i] * (attributeColors[i].a - maxHeight), 0);
		attributeColor += attributeColors[i] * weight;
		totalWeight += weight;
	}
	attributeColor /= totalWeight;
	//attributeColor = vec4(maxHeight);
	
#else
	// Do standard bilinear filtering
	vec4 attributeColor = vec4(0);
	for (int i = 0; i < 4; ++i)
	{
		int index = attrIndices[i];
		vec3 albedoDetailTexCoord = detailAlbedoUvScale[index] * detailTexCoordPerMeter;
		attributeColor += sampleDetailAlbedo(index, normal, albedoDetailTexCoord, normalUv) * attrWeights[i];
	}
#endif
	return attributeColor;
}

void main()
{
	vec2 normalUv = texCoord.xy * heightMapUvScale + heightMapUvOffset;
	vec3 normal = getTerrainNormalFromRG(texture(normalSampler, normalUv));
	normal = vec3(normal.z, normal.x, -normal.y);
	
#ifdef ADD_NORMAL_FBM_NOISE
	mat3 tbn = toTbnMatrix(normal);
	normal = fbmNormal() * tbn;
#endif

	float snowCoverage = 0.0;
	vec3 albedo = texture(overallAlbedoSampler, texCoord.xy * overallAlbedoMapUvScale + overallAlbedoMapUvOffset).rgb;


#ifdef ENABLE_DETAIL_ALBEDO_TEXTURES
	vec4 attributeColor = sampleAttributeDetailTextures(normal, normalUv);
	float albedoBlend = clamp(length(position_worldSpace) / 4000, 0.0, 1.0);
	vec3 filteredAlbedo = texture(attributeSampler, texCoord.xy * attributeMapUvScale + attributeMapUvOffset).rgb;
	albedo = mix(filteredAlbedo+1.0*(attributeColor.ggg-0.5), albedo, albedoBlend);
#endif
	vec3 viewDirection = normalize(cameraPosition - position_worldSpace);

#ifdef ENABLE_SHADOWS
	float lightVisibility = sampleShadowsAtWorldPosition(position_worldSpace);
#else
	float lightVisibility = 1.0;
#endif

#ifdef ENABLE_CLOUDS
	float cloudShadow = sampleCloudShadowMaskAtTerrainUv(cloudSampler, texCoord.xy, lightDirection);
	lightVisibility *= cloudShadow;
#endif
	vec3 visibleSunIrradiance = sunIrradiance * lightVisibility;
	
	vec3 specularReflectance;
	
	// Snow
	if (snowCoverage > 0)
	{
		specularReflectance = snowCoverage * calcBlinnPhongSpecular(lightDirection, viewDirection, normal, snowShininess) * visibleSunIrradiance;
	}

	// Water
	bool isWater = (elevation < 0);
	if (isWater)
	{
		normal = vec3(0,0,-1);
		specularReflectance = oceanSpecularColor * calcBlinnPhongSpecular(lightDirection, viewDirection, normal, oceanShininess) * visibleSunIrradiance;
		albedo = deepScatterColor;
	}
	else
	{
		specularReflectance = vec3(0);
	}

	vec3 totalReflectance = albedo * (
			calcLambertDirectionalLight(lightDirection, normal) * visibleSunIrradiance + 	
			calcLambertSkyLight(lightDirection, normal) * skyIrradiance * (lightVisibility*0.2+0.8) +
			ambientLightColor
		)
		+ specularReflectance;

	if (false && isWater) // FIXME: currently disabled because it makes the high-res terrain tiles not match the low-res planet tiles
	{
		vec3 upDir = vec3(0,0,-1);
		float fresnel = calcSchlickFresnel(dot(viewDirection, upDir));
		totalReflectance = mix(totalReflectance, skyIrradiance, fresnel);
	}

	color.rgb = totalReflectance * transmittance + skyRadianceToPoint;

//#define DEBUG_EDGES
#ifdef DEBUG_EDGES
	if(any(lessThan(texCoord.xy, vec2(0.02))))
	{
		color.rgb = vec3(1,0,0);
	}
#endif
	gl_FragDepth = logarithmicZ_fragmentShader(logZ);
	color.a = 1;
}