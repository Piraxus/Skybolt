/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ShadowHelpers.h"
#include "OsgStateSetHelpers.h"

namespace skybolt {
namespace vis {

void addShadowMapsToStateSet(const ShadowMaps& shadowMaps, osg::StateSet& stateSet, int firstTextureUnitIndex)
{
	for (int i = 0; i < (int)shadowMaps.size(); ++i)
	{
		stateSet.setTextureAttributeAndModes(firstTextureUnitIndex, shadowMaps[i]);
		stateSet.addUniform(createUniformSampler2dShadow("shadowSampler" + std::to_string(i), firstTextureUnitIndex++));
	}
}

} // namespace vis
} // namespace skybolt
