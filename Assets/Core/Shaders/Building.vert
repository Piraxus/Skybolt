/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#version 440 core
#pragma import_defines ( ENABLE_CLOUDS )

#include "DepthPrecision.h"
#include "AtmosphericScattering.h"
#include "CloudShadows.h"
#include "Noise/FastRandom.h"
#include "Shadows/Shadows.h"

in vec4 osg_Vertex;
in vec4 osg_Normal;
in vec4 osg_MultiTexCoord0;

out vec3 texCoord;
out vec3 normalWS;
out vec3 positionRelCamera;
out float logZ;
out vec3 sunIrradiance;
out vec3 skyIrradiance;
out vec3 transmittance;
out vec3 skyRadianceToPoint;
out vec3 colorMultiplier;
out vec3 shadowTexCoord;

uniform mat4 osg_ModelViewProjectionMatrix;
uniform mat4 modelMatrix;
uniform vec3 cameraPosition;
uniform vec3 lightDirection;

uniform sampler2D cloudSampler;

void main()
{
	gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;
	texCoord = osg_MultiTexCoord0.xyz;
	normalWS = mat3(modelMatrix) * osg_Normal.xyz;
	
	gl_Position.z = logarithmicZ_vertexShader(gl_Position.z, gl_Position.w, logZ);
	
	vec4 positionWS = modelMatrix * osg_Vertex;
	positionRelCamera = positionWS.xyz - cameraPosition;
	
	// Atmospheric scattering
	vec3 positionRelPlanet = positionWS.xyz - planetCenter;
	vec3 cameraPositionRelPlanet = cameraPosition - planetCenter;
	skyRadianceToPoint = GetSkyRadianceToPoint(cameraPositionRelPlanet, positionRelPlanet, 0, lightDirection, transmittance);
	sunIrradiance = GetSunAndSkyIrradiance(positionRelPlanet, lightDirection, skyIrradiance);
	
#ifdef ENABLE_CLOUDS
	sunIrradiance *= sampleCloudShadowMaskAtPositionRelPlanet(cloudSampler, positionRelPlanet, lightDirection);
#endif
	shadowTexCoord = (shadowProjectionMatrix0 * positionWS).xyz;
	colorMultiplier = vec3(0.6 + 0.5 * randomFast1d(osg_MultiTexCoord0.w));
}
