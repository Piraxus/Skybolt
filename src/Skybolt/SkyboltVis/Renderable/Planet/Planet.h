/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "PlanetSurface.h"
#include "SkyboltVis/RootNode.h"
#include "SkyboltVis/VisFactory.h"
#include "SkyboltVis/SkyboltVisFwd.h"
#include "SkyboltVis/Renderable/Atmosphere/Bruneton/BruentonAtmosphere.h"
#include "SkyboltVis/Renderable/Clouds/VolumeClouds.h"
#include "SkyboltVis/Renderable/Planet/Features/PlanetFeaturesSource.h"
#include "SkyboltVis/Renderable/Forest/GpuForest.h"
#include "SkyboltVis/Renderable/Water/WaterStateSet.h"

#include <SkyboltSim/SkyboltSimFwd.h>
#include <SkyboltSim/Spatial/LatLon.h>
#include <SkyboltCommon/File/FileLocator.h>

#include <osg/MatrixTransform>
#include <osg/Uniform>
#include <osg/Vec3f>

#include <boost/optional.hpp>

namespace skybolt {
namespace vis {

struct PlanetConfig
{
	px_sched::Scheduler* scheduler;
	const ShaderPrograms* programs;
	Scene* scene;
	sim::LatLon latLonOrigin;
	float innerRadius;
	PlanetTileSources planetTileSources;
	VisFactoryRegistry* visFactoryRegistry;
	int elevationMaxLodLevel = 1;
	int albedoMaxLodLevel = 1;
	int attributeMinLodLevel = 9;
	int attributeMaxLodLevel = 9;
	bool waterEnabled = true;
	osg::ref_ptr<osg::Texture2D> cloudsTexture; //!< Set to null to disable clouds
	boost::optional<BruentonAtmosphereConfig> atmosphereConfig;
	file::FileLocator fileLocator;
	std::vector<file::Path> featureTreeFiles;
	std::string featureTilesDirectoryRelAssetPackage;
	std::optional<ForestParams> forestParams;
};

class MyPlanetSurfaceListener;

class Planet : public RootNode
{
	friend MyPlanetSurfaceListener;
public:
	Planet(const PlanetConfig& config);
	~Planet();

	PlanetSurface* getSurface() const { return mPlanetSurface.get(); }

	void setJulianDate(double date)
	{
		mJulianDate = date;
	}

	void setCloudsVisible(bool visible);
	bool getCloudsVisible(void) const { return mCloudsVisible; }

	//! If set, clouds will have uniform coverage across planet.
	//! If not set, coverage will be governed by cloud texture.
	void setCloudCoverageFraction(boost::optional<float> cloudCoverageFraction);

	float getWaveHeight() const;

	void setWaveHeight(float height);

	//! Can return null
	osg::ref_ptr<WaterStateSet> getWaterStateSet() const
	{
		return mWaterStateSet;
	}

	float calcAtmosphericDensity(const osg::Vec3f& position) const;

public:
	// RootNode Implementation
	void setPosition(const osg::Vec3d &position) override;
	void setOrientation(const osg::Quat &orientation) override;

	osg::Vec3d getPosition() const override { return mTransform->getMatrix().getTrans(); }
	osg::Quat getOrientation() const override { return mTransform->getMatrix().getRotate(); }

	osg::Node* _getNode() const override;

	void updatePostSceneUpdate() override;
	void updatePreRender(const RenderContext& context) override;

private:
	void addTextureGeneratorToSceneGraph(const osg::ref_ptr<GpuTextureGenerator>& generator);
	void removeTextureGeneratorFromSceneGraph(const osg::ref_ptr<GpuTextureGenerator>& generator);

private:
	sim::World* mWorld;
	Scene* mScene;

	osg::ref_ptr<WaterStateSet> mWaterStateSet;
	OceanPtr mOcean;
	std::unique_ptr <class ReflectionCameraController> mReflectionCameraController;
	
	std::unique_ptr<class CascadedWaveHeightTextureGenerator> mWaveHeightTextureGenerator;

	osg::ref_ptr<GpuTextureGenerator> mEnvironmentMapGpuTextureGenerator;
	std::vector<osg::ref_ptr<GpuTextureGenerator>> mWaterSurfaceGpuTextureGenerators; //!< stored in execution order

	std::unique_ptr<PlanetSurface> mPlanetSurface;
	std::unique_ptr<PlanetSky> mPlanetSky;
	std::unique_ptr<PlanetFeatures> mPlanetFeatures;
	std::unique_ptr<VolumeClouds> mVolumeClouds;
	std::unique_ptr<class WaveFoamMaskGenerator> mWaveFoamMaskGenerator[WaterStateSetConfig::waveTextureCount];
	std::unique_ptr<class CascadedShadowMapGenerator> mShadowMapGenerator;
	std::unique_ptr<class BruentonAtmosphere> mAtmosphere;

	osg::Uniform* mPlanetCenterUniform;
	osg::Uniform* mPlanetMatrixInvUniform;

	double mInnerRadius;
	boost::optional<float> mAtmosphereScaleHeight;
	osg::ref_ptr<osg::MatrixTransform> mTransform;
	osg::ref_ptr<osg::MatrixTransform> mShadowSceneTransform;
	std::unique_ptr<MyPlanetSurfaceListener> mPlanetSurfaceListener;
	double mJulianDate = 0;
	osg::Uniform* mCloudDisplacementMetersUniform;
	osg::Uniform* mCloudCoverageFractionUniform;

	bool mCloudsVisible = false;
	bool mShadowsEnabled = true; // disabled because shadows are experimental
};

} // namespace vis
} // namespace skybolt
