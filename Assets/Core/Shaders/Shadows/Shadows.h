/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef SHADOWS_H
#define SHADOWS_H

float sampleShadow(sampler2D shadowSampler, vec2 texCoord, float receiverDepth)
{
	return texture(shadowSampler, texCoord).r > receiverDepth ? 1.0 : 0.0;
}

float sampleShadow(sampler2DShadow shadowSampler, vec2 texCoord, float receiverDepth)
{
	return texture(shadowSampler, vec3(texCoord, receiverDepth));
}

vec2 rotate(vec2 v, float a) {
	float s = sin(a);
	float c = cos(a);
	mat2 m = mat2(c, -s, s, c);
	return m * v;
}

float random(vec2 texCoord, float seed)
{
	float dot_product = dot(vec3(texCoord, seed), vec3(12.9898, 78.233,45.164));
	return fract(sin(dot_product) * 43758.5453);
}

// https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_standard_multisample_quality_levels
const vec2 standard4SamplePattern[4] = vec2[4]
(
	vec2(-2.0/8.0, -6.0/8.0),
	vec2(6.0/8.0, -2.0/8.0),
	vec2(-6.0/8.0, 2.0/8.0),
	vec2(2.0/8.0, 6.0/8.0)
);

const vec2 standard8SamplePattern[8] = vec2[8]
(
	vec2(1.0/8.0, -3.0/8.0),
	vec2(-1.0/8.0, 3.0/8.0),
	vec2(5.0 / 8.0, 1.0 / 8.0),
	vec2(-3.0 / 8.0, -5.0 / 8.0),
	vec2(-5.0 / 8.0, 5.0 / 8.0),
	vec2(-7.0 / 8.0, -1.0 / 8.0),
	vec2(3.0 / 8.0, 7.0 / 8.0),
	vec2(7.0 / 8.0, -7.0 / 8.0)
);

const vec2 poissonDisc16[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);

float sampleShadowPcf4StandardSamplePattern(sampler2DShadow shadowSampler, vec2 texCoord, float receiverDepth)
{
	vec2 offsetScale = 1.0 / textureSize(shadowSampler, 0);
	
	float result = 0.0f;
	const int sampleCount = 4;
	for (int i = 0; i < sampleCount; ++i)
	{
		vec2 offset = standard4SamplePattern[i];
		result += sampleShadow(shadowSampler, texCoord + offset * offsetScale, receiverDepth);
	}

	return result / sampleCount;
}

float sampleShadowPcf3x3(sampler2DShadow shadowSampler, vec2 texCoord, float receiverDepth)
{
	vec2 offsetScale = 1.0 / textureSize(shadowSampler, 0);
	
	float result = 0.0f;
	for (int j = -1; j <= 1; ++j)
	{
		for (int i = -1; i <= 1; ++i)
		{
			vec2 offset = vec2(i, j);
			result += sampleShadow(shadowSampler, texCoord + offset * offsetScale, receiverDepth);
		}
	}

	return result / 9;
}

float sampleShadowPcf8StandardSamplePattern(sampler2DShadow shadowSampler, vec2 texCoord, float receiverDepth)
{
	vec2 offsetScale = 1.0 / textureSize(shadowSampler, 0);
	
	float result = 0.0f;
	const int sampleCount = 8;
	for (int i = 0; i < sampleCount; ++i)
	{
		vec2 offset = standard8SamplePattern[i];
		result += sampleShadow(shadowSampler, texCoord + offset * offsetScale, receiverDepth);
	}

	return result / sampleCount;
}

// 'Optimized PCF' method used in The Witness.
// Based on http://the-witness.net/news/2013/09/shadow-mapping-summary-part-1
// See sample code here https://github.com/TheRealMJP/Shadows
// The method weights each PCF sample with a Gaussian curve which gives smoother
// edges for the same number of samples compared with grid-based PCF.
float sampleShadowOptimizedPcf(sampler2DShadow shadowSampler, vec2 texCoord, float receiverDepth)
{
	vec2 offsetScale = 1.0 / textureSize(shadowSampler, 0);
	
	vec2 uv = texCoord * textureSize(shadowSampler, 0);
	vec2 base_uv;
    base_uv.x = floor(uv.x + 0.5);
    base_uv.y = floor(uv.y + 0.5);
	float s = (uv.x + 0.5 - base_uv.x);
    float t = (uv.y + 0.5 - base_uv.y);
	base_uv -= vec2(0.5, 0.5);

	float sum = 0.0f;
	
#define OPTIMIZED_PCF_FILTER_SIZE 5
#if OPTIMIZED_PCF_FILTER_SIZE == 3
	float uw0 = (3 - 2 * s);
	float uw1 = (1 + 2 * s);

	float u0 = (2 - s) / uw0 - 1;
	float u1 = s / uw1 + 1;

	float vw0 = (3 - 2 * t);
	float vw1 = (1 + 2 * t);

	float v0 = (2 - t) / vw0 - 1;
	float v1 = t / vw1 + 1;

	sum += uw0 * vw0 * sampleShadow(shadowSampler, (base_uv + vec2(u0, v0)) * offsetScale, receiverDepth);
	sum += uw1 * vw0 * sampleShadow(shadowSampler, (base_uv + vec2(u1, v0)) * offsetScale, receiverDepth);
	sum += uw0 * vw1 * sampleShadow(shadowSampler, (base_uv + vec2(u0, v1)) * offsetScale, receiverDepth);
	sum += uw1 * vw1 * sampleShadow(shadowSampler, (base_uv + vec2(u1, v1)) * offsetScale, receiverDepth);

	return sum * 1.0f / 16;
#elif OPTIMIZED_PCF_FILTER_SIZE == 5
	float uw0 = (4 - 3 * s);
	float uw1 = 7;
	float uw2 = (1 + 3 * s);

	float u0 = (3 - 2 * s) / uw0 - 2;
	float u1 = (3 + s) / uw1;
	float u2 = s / uw2 + 2;

	float vw0 = (4 - 3 * t);
	float vw1 = 7;
	float vw2 = (1 + 3 * t);

	float v0 = (3 - 2 * t) / vw0 - 2;
	float v1 = (3 + t) / vw1;
	float v2 = t / vw2 + 2;

	sum += uw0 * vw0 * sampleShadow(shadowSampler, (base_uv + vec2(u0, v0)) * offsetScale, receiverDepth);
	sum += uw1 * vw0 * sampleShadow(shadowSampler, (base_uv + vec2(u1, v0)) * offsetScale, receiverDepth);
	sum += uw2 * vw0 * sampleShadow(shadowSampler, (base_uv + vec2(u2, v0)) * offsetScale, receiverDepth);

	sum += uw0 * vw1 * sampleShadow(shadowSampler, (base_uv + vec2(u0, v1)) * offsetScale, receiverDepth);
	sum += uw1 * vw1 * sampleShadow(shadowSampler, (base_uv + vec2(u1, v1)) * offsetScale, receiverDepth);
	sum += uw2 * vw1 * sampleShadow(shadowSampler, (base_uv + vec2(u2, v1)) * offsetScale, receiverDepth);

	sum += uw0 * vw2 * sampleShadow(shadowSampler, (base_uv + vec2(u0, v2)) * offsetScale, receiverDepth);
	sum += uw1 * vw2 * sampleShadow(shadowSampler, (base_uv + vec2(u1, v2)) * offsetScale, receiverDepth);
	sum += uw2 * vw2 * sampleShadow(shadowSampler, (base_uv + vec2(u2, v2)) * offsetScale, receiverDepth);

	return sum * 1.0f / 144;
#endif
}

float sampleShadowPoisson16(sampler2DShadow shadowSampler, vec2 texCoord, float receiverDepth)
{
	vec2 offsetScale = 1.5 / textureSize(shadowSampler, 0);
	
	float result = 0.0f;
	const int sampleCount = 16;
	for (int i = 0; i < sampleCount; ++i)
	{
		vec2 offset = poissonDisc16[i];
		result += sampleShadow(shadowSampler, texCoord + offset * offsetScale, receiverDepth);
	}

	return result / sampleCount;
}

bool inTextureBounds(vec2 texCoord)
{
	vec2 v = abs(texCoord - vec2(0.5));
	return max(v.x, v.y) < 0.5;
}

uniform sampler2DShadow shadowSampler0;
uniform sampler2DShadow shadowSampler1;
uniform sampler2DShadow shadowSampler2;
uniform sampler2DShadow shadowSampler3;

uniform mat4 shadowProjectionMatrix0;
uniform mat4 shadowProjectionMatrix1;
uniform vec4 cascadeShadowMatrixModifier1;
uniform vec4 cascadeShadowMatrixModifier2;
uniform vec4 cascadeShadowMatrixModifier3;

float sampleShadowsAtTexCoord(vec2 shadowTexcoord, float receiverDepth)
{
	vec2 v = abs(shadowTexcoord - vec2(0.5));
	float d = max(v.x, v.y);
	
	if (d  < 0.5)
	{
		return sampleShadowOptimizedPcf(shadowSampler0, shadowTexcoord.xy, receiverDepth);
		//return sampleShadow(shadowSampler0, shadowTexcoord.xy, receiverDepth);
	}
	else
	{
		vec2 cascadeTecoord = shadowTexcoord * cascadeShadowMatrixModifier1.x + cascadeShadowMatrixModifier1.yz;
		float cascadeReceiverDepth = receiverDepth + cascadeShadowMatrixModifier1.w;
	
		v = abs(cascadeTecoord - vec2(0.5));
		d = max(v.x, v.y);
		if (d  < 0.5)
		{
			float weight = 0.0;// max(0.0, d * 4.0 - 1.0);
			return mix(sampleShadowOptimizedPcf(shadowSampler1, cascadeTecoord.xy, cascadeReceiverDepth), 1.0, weight);
			//return mix(sampleShadow(shadowSampler1, cascadeTecoord.xy, cascadeReceiverDepth), 1.0, weight);
		}
	}
	return 1.0;
}

float sampleShadowsAtWorldPosition(vec3 positionWorldSpace)
{
	vec3 shadowTexcoord = (shadowProjectionMatrix0 * vec4(positionWorldSpace, 1)).xyz;
	float receiverDepth = 0.99; // TODO
	return sampleShadowsAtTexCoord(shadowTexcoord.xy, receiverDepth);
}

#endif // SHADOWS_H