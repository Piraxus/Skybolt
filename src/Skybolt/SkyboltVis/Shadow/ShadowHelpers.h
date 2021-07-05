/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <osg/Texture2D>
#include <vector>

namespace skybolt {
namespace vis {

typedef std::vector<osg::ref_ptr<osg::Texture2D>> ShadowMaps;

void addShadowMapsToStateSet(const ShadowMaps& shadowMaps, osg::StateSet& stateSet, int firstTextureUnitIndex);

} // namespace vis
} // namespace skybolt
