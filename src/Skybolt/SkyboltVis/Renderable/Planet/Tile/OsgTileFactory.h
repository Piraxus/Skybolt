/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "OsgTile.h"
#include "TileTexture.h"
#include "SkyboltVis/SkyboltVisFwd.h"
#include "SkyboltVis/Renderable/Planet/Terrain.h"
#include "SkyboltVis/Shadow/ShadowHelpers.h"
#include <SkyboltCommon/Math/QuadTree.h>

#include <osg/Image>
#include <map>

namespace skybolt {
namespace vis {

struct OsgTileFactoryConfig
{
	const ShaderPrograms* programs;
	ShadowMaps shadowMaps;
	std::vector<osg::ref_ptr<osg::Texture2D>> albedoDetailMaps;

	double planetRadius;
	bool hasCloudShadows;
};

class OsgTileFactory
{
public:
	OsgTileFactory(const OsgTileFactoryConfig& config);
	~OsgTileFactory();

	struct TileTextures
	{
		TileTexture height;
		osg::ref_ptr<osg::Texture2D> normal; //!< Same key as height texture
		osg::ref_ptr<osg::Texture2D> landMask; //!< Same key as height texture
		TileTexture albedo;
		std::optional<TileTexture> attribute;
	};

	OsgTile createOsgTile(const skybolt::QuadTreeTileKey& key, const Box2d& latLonBounds, const TileTextures& textures) const;

private:
	double mPlanetRadius;
	const ShaderPrograms* mPrograms;
	ShadowMaps mShadowMaps;
	std::vector<osg::ref_ptr<osg::Texture2D>> mAlbedoDetailMaps;
	bool mHasCloudShadows;
};

} // namespace vis
} // namespace skybolt
