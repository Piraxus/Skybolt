/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

vec3 getNormalFromDxt5n(vec4 color)
{
	vec2 xy = color.ag * 2 - 1;
	float z = sqrt(1.0f - xy.x * xy.x - xy.y * xy.y);
	return vec3(xy.x, xy.y, z);
}

vec3 getTerrainNormalFromDxt5n(vec4 color)
{
	vec2 xz = color.ag * 2 - 1;
	float y = sqrt(1.0f - xz.x * xz.x - xz.y * xz.y);
	return vec3(xz.x, y, xz.y);
}

vec3 getTerrainNormalFromRG(vec4 color)
{
	vec2 xz = color.rg * 2 - 1;
	float y = sqrt(max(0, 1.0f - dot(xz, xz)));
	return vec3(xz.x, y, xz.y);
}

mat3 getTbn(vec3 tangentN, vec3 normalN)
{
	vec3 bitangent = cross(tangentN, normalN);
	return mat3(tangentN, bitangent, normalN);
}
