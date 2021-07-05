/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "DefaultRootNode.h"
#include "Shadow/ShadowHelpers.h"

namespace skybolt {
namespace vis {

struct Building
{
	std::vector<osg::Vec3f> points; //!< Points must be in clockwise order
	float height;
};

typedef std::vector<Building> Buildings;

class BuildingsBatch : public DefaultRootNode
{
public:

	struct Uniforms
	{
		osg::Uniform* modelMatrix;
	};

	BuildingsBatch(const Buildings& buildings, const osg::ref_ptr<osg::Program>& program, const ShadowMaps& shadowMaps);

protected:
	void updatePreRender(const RenderContext& context);

private:
	Uniforms mUniforms;
};

} // namespace vis
} // namespace skybolt
