/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <osg/Uniform>

namespace skybolt {
namespace vis {

inline osg::Uniform* createUniformSampler2d(const std::string& name, int id)
{
	osg::Uniform* uniform = new osg::Uniform(osg::Uniform::SAMPLER_2D, name);
	uniform->set(id);
	return uniform;
}

inline osg::Uniform* createUniformSampler2dShadow(const std::string& name, int id)
{
	osg::Uniform* uniform = new osg::Uniform(osg::Uniform::SAMPLER_2D_SHADOW, name);
	uniform->set(id);
	return uniform;
}

inline osg::Uniform* createArrayOfUniformSampler2d(const std::string& name, int firstId, int numElements)
{
	osg::Uniform* uniform = new osg::Uniform(osg::Uniform::SAMPLER_2D, name, numElements);
	for (int i = 0; i < numElements; ++i)
	{
		uniform->setElement(i, firstId + i);
	}
	return uniform;
}

inline osg::Uniform* createUniformSampler2dArray(const std::string& name, int id)
{
	osg::Uniform* uniform = new osg::Uniform(osg::Uniform::SAMPLER_2D_ARRAY, name);
	uniform->set(id);
	return uniform;
}

inline osg::Uniform* createUniformSampler3d(const std::string& name, int id)
{
	osg::Uniform* uniform = new osg::Uniform(osg::Uniform::SAMPLER_3D, name);
	uniform->set(id);
	return uniform;
}

inline osg::Uniform* createUniformSamplerTbo(const std::string& name, int id)
{
	osg::Uniform* uniform = new osg::Uniform(osg::Uniform::SAMPLER_BUFFER, name);
	uniform->set(id);
	return uniform;
}

void makeStateSetTransparent(osg::StateSet& stateSet);

} // namespace vis
} // namespace skybolt
