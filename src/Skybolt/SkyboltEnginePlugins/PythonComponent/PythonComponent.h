/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <SkyboltEngine/SkyboltEngineFwd.h>
#include <SkyboltSim/SkyboltSimFwd.h>
#include <SkyboltSim/Component.h>
#include <string>

namespace skybolt {

class PythonComponent : public sim::Component
{
public:
	PythonComponent(sim::Entity* entity, JulianDateProvider julianDateProvider, const std::string& moduleName, const std::string& className);
	~PythonComponent();

	void updatePreDynamics(sim::TimeReal dt, sim::TimeReal dtWallClock) override;

private:
	sim::Entity* mEntity;
	std::unique_ptr<struct PythonData> mPythonData;
	JulianDateProvider mJulianDateProvider;
};

} // namespace skybolt