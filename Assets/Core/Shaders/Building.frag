/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#version 440 core
#pragma import_defines ( ENABLE_SHADOWS )

#include "DepthPrecision.h"
#include "Brdfs/BlinnPhong.h"
#include "Brdfs/Lambert.h"
#include "Shadows/Shadows.h"

in vec3 texCoord;
in vec3 normalWS;
in vec3 positionRelCamera;
in float logZ;
in vec3 sunIrradiance;
in vec3 skyIrradiance;
in vec3 transmittance;
in vec3 skyRadianceToPoint;
in vec3 colorMultiplier;
in vec3 shadowTexCoord;

out vec4 color;

uniform sampler2DArray albedoSampler;
uniform vec3 lightDirection;
uniform vec3 ambientLightColor;

const float shininess = 32;
const float specularity = 0.3;

void main()
{
	color = texture(albedoSampler, texCoord.xyz);
	color.rgb *= colorMultiplier;
	
#ifdef ENABLE_SHADOWS
	float lightVisibility = sampleShadowsAtTexCoord(shadowTexCoord.xy, shadowTexCoord.z - 0.001);
#else
	float lightVisibility = 1.0;
#endif
	
	vec3 visibleSunIrradiance = sunIrradiance * lightVisibility;
	
	color.rgb *= calcLambertDirectionalLight(lightDirection, normalWS) * visibleSunIrradiance
		+ calcLambertSkyLight(lightDirection, normalWS) * skyIrradiance
		+ ambientLightColor;
	
	vec3 viewDirection = normalize(-positionRelCamera);
	color.rgb += visibleSunIrradiance * specularity * vec3(calcBlinnPhongSpecular(lightDirection, viewDirection, normalWS, shininess));
	
	color.rgb = color.rgb * transmittance + skyRadianceToPoint;
	
	gl_FragDepth = logarithmicZ_fragmentShader(logZ);
}
