/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#version 330 core
#pragma import_defines ( CAST_SHADOWS )
#pragma import_defines ( ENABLE_SHADOWS )

#include "DepthPrecision.h"
#include "Trees.h"
#include "Brdfs/Lambert.h"
#include "Shadows/Shadows.h"

in vec2 texCoord;
in float perTreeUnitRandom;
in vec3 normal;
in float logZ;
in vec3 sunIrradiance;
in vec3 skyIrradiance;
in vec3 transmittance;
in vec3 skyRadianceToPoint;
in vec3 shadowTexCoord;
in float fragmentViewDistance;
out vec4 color;

uniform sampler2D albedoSampler;
uniform vec3 lightDirection;
uniform vec3 ambientLightColor;

void main()
{
	color = texture(albedoSampler, texCoord.xy);

	if (color.a < 0.5)
		discard;

#ifdef CAST_SHADOWS
	return;
#endif

#ifdef ENABLE_SHADOWS
	float dotLN = 1.0;
	float lightVisibility = sampleShadowsAtTexCoord(shadowTexCoord.xy, shadowTexCoord.z-0.0004, dotLN, fragmentViewDistance);
#else
	float lightVisibility = 1.0;
#endif
	
	color.rgb = randomizeColor(color.rgb, perTreeUnitRandom)*vec3(0.7,1,1.2);
	color.rgb *= calcLambertSkyLight(lightDirection, normal)
	* (sunIrradiance * lightVisibility + skyIrradiance) + ambientLightColor;	
	color.rgb = color.rgb * transmittance + skyRadianceToPoint;
	
	gl_FragDepth = logarithmicZ_fragmentShader(logZ);
}