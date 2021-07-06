/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Planet.h"
#include "SkyboltVis/GlobalSamplerUnit.h"
#include "SkyboltVis/OsgTextureHelpers.h"
#include "SkyboltVis/RenderContext.h"
#include "SkyboltVis/Camera.h"
#include "SkyboltVis/Light.h"
#include "SkyboltVis/Scene.h"
#include "SkyboltVis/RenderTarget/RenderTexture.h"
#include "SkyboltVis/Shader/ShaderProgramRegistry.h"
#include "SkyboltVis/Window/Window.h"
#include "SkyboltVis/TextureGenerator/GpuTextureGenerator.h"
#include "SkyboltVis/TextureGenerator/GpuTextureGeneratorStateSets.h"
#include "SkyboltVis/Renderable/Atmosphere/Bruneton/BruentonAtmosphere.h"
#include "SkyboltVis/Renderable/Planet/PlanetSurface.h"
#include "SkyboltVis/Renderable/Planet/PlanetSky.h"
#include "SkyboltVis/Renderable/Planet/Terrain.h"
#include "SkyboltVis/Renderable/Planet/Features/PlanetFeatures.h"
#include "SkyboltVis/Renderable/Planet/Tile/OsgTileFactory.h"
#include "SkyboltVis/Renderable/Billboard.h"
#include "SkyboltVis/Renderable/Water/ReflectionCameraController.h"
#include "SkyboltVis/Renderable/Water/Ocean.h"
#include "SkyboltVis/Renderable/Water/WaveHeightTextureGenerator.h"
#include "SkyboltVis/Shadow/CascadedShadowMapGenerator.h"

#include "SkyboltVis/MatrixHelpers.h"
#include "SkyboltVis/OsgImageHelpers.h"
#include "SkyboltVis/OsgStateSetHelpers.h"

#include <SkyboltSim/Physics/Astronomy.h>

#include <SkyboltCommon/Exception.h>
#include <SkyboltCommon/Math/MathUtility.h>

#include <osgDB/ReadFile>

#include <future>

using namespace skybolt::sim;

namespace skybolt {
namespace vis {

static osg::ref_ptr<osg::Texture2D> createReflectionTexture(int textureWidth, int textureHeight)
{
	//Create the texture image
	osg::Image* image = new osg::Image();
	image->allocateImage(textureWidth,
		textureHeight,
		1,   // 2D texture is 1 pixel deep
		GL_RGBA,
		GL_UNSIGNED_BYTE);
	image->setInternalTextureFormat(GL_RGBA8);

	// Create the texture object
	osg::ref_ptr<osg::Texture2D> texture = createRenderTexture(textureWidth, textureHeight);
	texture->setResizeNonPowerOfTwoHint(false);
	texture->setImage(image);
	texture->setInternalFormat(GL_RGBA);
	texture->setFilter(osg::Texture2D::FilterParameter::MIN_FILTER, osg::Texture2D::FilterMode::LINEAR);
	texture->setFilter(osg::Texture2D::FilterParameter::MAG_FILTER, osg::Texture2D::FilterMode::LINEAR);
	texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
	texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

	return texture;
}

static osg::StateSet* createTexturedQuadStateSet(osg::ref_ptr<osg::Program> program, osg::ref_ptr<osg::Texture> texture)
{
	osg::StateSet* stateSet = new osg::StateSet();
	stateSet->setAttributeAndModes(program, osg::StateAttribute::ON);
	stateSet->setTextureAttributeAndModes(0, texture, osg::StateAttribute::ON);
	stateSet->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
	return stateSet;
}

static osg::StateSet* createSkyToEnvironmentMapStateSet(osg::ref_ptr<osg::Program> program)
{
	osg::StateSet* stateSet = new osg::StateSet();
	stateSet->setAttributeAndModes(program, osg::StateAttribute::ON);
	return stateSet;
}

static osg::ref_ptr<osg::Texture2D> createNormalTexture(int width, int height)
{
	osg::ref_ptr<osg::Texture2D> texture = createRenderTexture(width, height);
	texture->setInternalFormat(GL_RGBA);
	texture->setFilter(osg::Texture2D::FilterParameter::MIN_FILTER, osg::Texture2D::FilterMode::LINEAR_MIPMAP_LINEAR);
	texture->setFilter(osg::Texture2D::FilterParameter::MAG_FILTER, osg::Texture2D::FilterMode::LINEAR);
	texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
	texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);

	return texture;
}

osg::Texture2D* createFoamMaskTexture(int width, int height)
{
	// Create the texture object and set the image
	osg::ref_ptr<osg::Image> image = new osg::Image();
	image->allocateImage(width, height, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE);
	memset(image->data(), 0, width * height);
	osg::Texture2D* texture = new osg::Texture2D(image);
	texture->setTextureWidth(width);
	texture->setTextureHeight(height);
	texture->setInternalFormat(GL_RED);
	texture->setFilter(osg::Texture2D::FilterParameter::MIN_FILTER, osg::Texture2D::FilterMode::LINEAR_MIPMAP_LINEAR);
	texture->setFilter(osg::Texture2D::FilterParameter::MAG_FILTER, osg::Texture2D::FilterMode::LINEAR);
	texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
	texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);

	return texture;
}

static osg::ref_ptr<osg::Texture2D> createEnvironmentTexture(int width, int height)
{
	osg::ref_ptr<osg::Texture2D> texture = createRenderTexture(width, height);
	texture->setInternalFormat(GL_RGB16F_ARB);
	texture->setFilter(osg::Texture2D::FilterParameter::MIN_FILTER, osg::Texture2D::FilterMode::LINEAR_MIPMAP_LINEAR);
	texture->setFilter(osg::Texture2D::FilterParameter::MAG_FILTER, osg::Texture2D::FilterMode::LINEAR);
	texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
	texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

	return texture;
}

//! @param textureScale is textureSizeInWorldSpace / maxHeightInWorldSpace
osg::StateSet* createHeightToNormalMapStateSet(osg::ref_ptr<osg::Program> program, osg::Texture2D* heightTexture, const osg::Vec2f& textureScale)
{
	osg::StateSet* stateSet = new osg::StateSet();
	stateSet->setAttributeAndModes(program, osg::StateAttribute::ON);
	stateSet->setTextureAttributeAndModes(0, heightTexture, osg::StateAttribute::ON);

	const osg::Image& image = *heightTexture->getImage(0);
	osg::Vec2f texelSize(1.0f / (float)image.s(), 1.0f / (float)image.t());
	osg::Vec2f texelScale(textureScale.x() * texelSize.x(), textureScale.y() * texelSize.y());
	stateSet->addUniform(new osg::Uniform("texelSizeInTextureSpace", texelSize));
	stateSet->addUniform(new osg::Uniform("texelScale", texelScale));

	return stateSet;
}

bool flatEarth = false;

class MyPlanetSurfaceListener : public PlanetSurfaceListener
{
public:
	MyPlanetSurfaceListener(Planet* planet) :
		mPlanet(planet)
	{}

	void tileAddedToSceneGraph(const skybolt::QuadTreeTileKey& key) override
	{
		mPlanet->mPlanetFeatures->onSurfaceTileAdded(key);
	}

	Planet* mPlanet;
};

class CascadedWaveHeightTextureGenerator
{
public:
	CascadedWaveHeightTextureGenerator(const WaveHeightTextureGeneratorFactory& factory, float smallestTextureWorldSize)
	{
		for (int i = 0; i < numCascades; ++i)
		{
			float scale = (i == 0) ? 1 : 5;
			float worldSize = smallestTextureWorldSize * scale;
			glm::vec2 normalizedFrequencyRange = glm::vec2(0.0, 1);

			// If this is not the last cascade, cut off the lower part of the frequency range so that the frequencies are not repeated in other cascades
			if (i < numCascades - 1)
			{
				normalizedFrequencyRange = glm::vec2(0.2, 1);
			}

			mCascades[i] = factory.create(worldSize, normalizedFrequencyRange);
			mWorldSizes[i] = worldSize;
		}
	}

	bool update(double time)
	{
		bool updated = false;
		for (int i = 0; i < numCascades; ++i)
		{
			if (mCascades[i]->generate(time))
			{
				updated = true;
			}
		}
		return updated;
	}

	float getWorldSize(int i) const
	{
		return mWorldSizes[i];
	}

	osg::ref_ptr<osg::Texture2D> getTexture(int cascade) const
	{
		return mCascades[cascade]->getTexture();
	}

	float getWaveHeight() const
	{
		return mCascades[0]->getWaveHeight();
	}

	void setWaveHeight(float height)
	{
		for (const auto& i : mCascades)
		{
			i->setWaveHeight(height);
		}
	}

	static const int numCascades = 1;

private:
	std::unique_ptr<WaveHeightTextureGenerator> mCascades[numCascades];
	float mWorldSizes[numCascades];
};

static_assert(WaterStateSetConfig::waveTextureCount == CascadedWaveHeightTextureGenerator::numCascades, "Number of wave image cascades should match number of textures");

struct WaveFoamMaskGeneratorConfig
{
	osg::ref_ptr<osg::Program> program;
	osg::ref_ptr<osg::Texture2D> waveVectorDisplacementTexture;
	osg::Vec2f textureWorldSize;
	osg::Vec2i textureSizePixels;
	osg::Group* parent;
};

//! Generates a foam mask texture from a wave vector displacement texture.
//! There are two output buffers which are ping-ponged so that the output from the previous
//! frame can be used as an input to the next. In this way, the foam can be made to fade out
//! slowly over time.
class WaveFoamMaskGenerator
{
public:
	WaveFoamMaskGenerator(const WaveFoamMaskGeneratorConfig& config) :
		mSwitch(new osg::Switch()),
		mIndex(0)
	{
		const float jacobianLambda = 1.0f;
		const float generateMipMaps = true;

		for (int i = 0; i < 2; ++i)
		{
			mOutputTexture[i] = createFoamMaskTexture(config.textureSizePixels.x(), config.textureSizePixels.y());
		}

		for (int i = 0; i < 2; ++i)
		{
			const int otherIndex = !i;
			mGenerator[i] = new GpuTextureGenerator(mOutputTexture[i], createWaveFoamMaskGeneratorStateSet(config.program, config.waveVectorDisplacementTexture, mOutputTexture[otherIndex], config.textureWorldSize, jacobianLambda), generateMipMaps);
			mSwitch->addChild(mGenerator[i], i == 0);
		}

		config.parent->addChild(mSwitch);

		mFoamMaskSubtractionAmountUniform = new osg::Uniform("foamMaskSubtractionAmount", 0.0f);
		mSwitch->getOrCreateStateSet()->addUniform(mFoamMaskSubtractionAmountUniform);
	}

	~WaveFoamMaskGenerator()
	{
		mSwitch->getParent(0)->removeChild(mSwitch);
	}

	void swapBuffers()
	{
		mIndex = (mIndex + 1) % 2;
		mSwitch->setValue(0, mIndex);
		mSwitch->setValue(1, !mIndex);
	}

	void advanceTime(double timeSeconds)
	{
		// Update subtraction amount for previous frame's foam mask when mixing with the current frame.
		// This makes old foam slowly fade out over time.
		const float totalFadeTime = 30.0f;
		float amount = std::min(1.0, std::abs(timeSeconds - mPrevTimeSeconds)) / totalFadeTime;
		// Don't fade this frame if the amount is less than the minimum value that has any effect in 8 bit textures.
		if (amount >= 0.005)
		{
			mPrevTimeSeconds = timeSeconds;
		}
		else
		{
			amount = 0;
		}
		mFoamMaskSubtractionAmountUniform->set(amount);

		mGenerator[mIndex]->requestRender();
	}

	osg::ref_ptr<osg::Texture2D> getOutputTexture() const
	{
		return mOutputTexture[mIndex];
	}

private:
	osg::ref_ptr<osg::Switch> mSwitch;
	// Two outputs so we can ping-pong between them.
	// This is required for feeding the output of the previous into the input of the next
	// so that the foam can persist over time.
	osg::ref_ptr<GpuTextureGenerator> mGenerator[2];
	osg::ref_ptr<osg::Texture2D> mOutputTexture[2];
	int mIndex;

	osg::ref_ptr<osg::Uniform> mFoamMaskSubtractionAmountUniform;
	double mPrevTimeSeconds = 0;
};

// Creates a 'textureizer map' which is a detail map that can be added
// to a lower detail base map.
// To produce a texturizer map, we take an original detail map, find the average,
// subtract the average, and add 0.5 to all channels. The result is a map that contains
// just the offsets to apply the original texture to a base map while preserving the average
// color of the base map.
// TODO: need to test this more to see if it is worth using. Currently unused.
static void convertSrgbToTexturizerMap(osg::Image& image)
{
	float alphaRejectionThreshold = -1.0; // don't reject alpha
	osg::Vec4f averageLinearSpace = srgbToLinear(averageSrgbColor(image, alphaRejectionThreshold));

	osg::Vec4f color;
	size_t pixels = 0;
	for (size_t t = 0; t < image.t(); ++t)
	{
		for (size_t s = 0; s < image.s(); ++s)
		{
			osg::Vec4f originalColor = image.getColor(s, t);
			osg::Vec4f c = srgbToLinear(originalColor);
			c = c - averageLinearSpace + osg::Vec4f(0.5, 0.5, 0.5, 0.5);
			c = linearToSrgb(c);
			c.a() = originalColor.a(); // perserve alpha
			image.setColor(c, s, t);
		}
	}
}


Planet::Planet(const PlanetConfig& config) :
	mScene(config.scene),
	mInnerRadius(config.innerRadius),
	mTransform(new osg::MatrixTransform),
	mShadowSceneTransform(new osg::MatrixTransform),
	mPlanetSurfaceListener(new MyPlanetSurfaceListener(this))
{
	if (config.atmosphereConfig)
	{
		mAtmosphereScaleHeight = config.atmosphereConfig->rayleighScaleHeight;

		mAtmosphere = std::make_unique<BruentonAtmosphere>(*config.atmosphereConfig);
		mScene->addObject(mAtmosphere.get());

		mTransform->getOrCreateStateSet()->setDefine("ENABLE_ATMOSPHERE");

		PlanetSkyConfig skyConfig;
		skyConfig.program = config.programs->getRequiredProgram("sky");
		skyConfig.radius = config.atmosphereConfig->topRadius;
		mPlanetSky.reset(new PlanetSky(skyConfig));
		mScene->addObject(mPlanetSky.get());
	}

	// Global uniforms, to be shared for everything in scene
	{
		osg::StateSet* ss = mScene->_getGroup()->getOrCreateStateSet();
		mPlanetCenterUniform = new osg::Uniform("planetCenter", osg::Vec3f(0, 0, 0));
		ss->addUniform(mPlanetCenterUniform);

		mPlanetMatrixInvUniform = new osg::Uniform("planetMatrixInv", osg::Matrixf());
		ss->addUniform(mPlanetMatrixInvUniform);

		ss->addUniform(new osg::Uniform("innerRadius", (float)mInnerRadius));

		mCloudDisplacementMetersUniform = new osg::Uniform("cloudDisplacementMeters", osg::Vec2f(0, 0));
		ss->addUniform(mCloudDisplacementMetersUniform);

		mCloudCoverageFractionUniform = new osg::Uniform("cloudCoverageFraction", 0.5f);
		ss->addUniform(mCloudCoverageFractionUniform);
	}

	// Create sky environment sphere
	osg::ref_ptr<osg::Texture2D> environmentTexture = createEnvironmentTexture(128, 128);
	mEnvironmentMapGpuTextureGenerator = new GpuTextureGenerator(environmentTexture, createSkyToEnvironmentMapStateSet(config.programs->getRequiredProgram("skyToEnvironmentMap")), /* generateMipMaps */ true);
	addTextureGeneratorToSceneGraph(mEnvironmentMapGpuTextureGenerator);

	int shadowCascades = 3;
	mShadowMapGenerator = std::make_unique<CascadedShadowMapGenerator>(config.programs->getRequiredProgram("shadowCaster"), shadowCascades);

	// Create terrain
	{
		std::shared_ptr<OsgTileFactory> osgTileFactory;
		{
			OsgTileFactoryConfig factoryConfig;
			factoryConfig.programs = config.programs;
			factoryConfig.shadowMaps = mShadowMapGenerator->getTextures();
			factoryConfig.planetRadius = mInnerRadius;
			factoryConfig.hasCloudShadows = config.cloudsTexture != nullptr;
			osgTileFactory = std::make_shared<OsgTileFactory>(factoryConfig);
		}

		GpuForestPtr forest;
		if (config.forestParams)
		{
			vis::GpuForestConfig forestConfig;
			forestConfig.forestParams = *config.forestParams;
			forestConfig.parentGroup = config.scene->_getGroup();
			forestConfig.parentTransform = mTransform;
			forestConfig.planetRadius = mInnerRadius;
			forestConfig.programs = config.programs;
			forest = std::make_shared<vis::GpuForest>(forestConfig);
		}

		PlanetSurfaceConfig surfaceConfig;
		surfaceConfig.scheduler = config.scheduler;
		surfaceConfig.programs = config.programs;
		surfaceConfig.radius = mInnerRadius;
		surfaceConfig.osgTileFactory = osgTileFactory;
		surfaceConfig.parentTransform = mTransform;
		surfaceConfig.gpuForest = forest;
		surfaceConfig.planetTileSources = config.planetTileSources;
		surfaceConfig.oceanEnabled = config.waterEnabled;
		surfaceConfig.cloudsTexture = config.cloudsTexture;
		surfaceConfig.elevationMaxLodLevel = config.elevationMaxLodLevel;
		surfaceConfig.albedoMaxLodLevel = config.albedoMaxLodLevel;
		surfaceConfig.attributeMinLodLevel = config.attributeMinLodLevel;
		surfaceConfig.attributeMaxLodLevel = config.attributeMaxLodLevel;

		mPlanetSurface.reset(new PlanetSurface(surfaceConfig));
	}

	// Create water state set
	if (config.waterEnabled)
	{
		auto it = config.visFactoryRegistry->find(VisFactoryType::WaveHeightTextureGenerator);
		if (it != config.visFactoryRegistry->end())
		{
			WaterStateSetConfig stateSetConfig;
			stateSetConfig.environmentTexture = environmentTexture;

			float smallestWaveHeightMapWorldSize = 500.0f; // FIXME: To avoid texture wrapping issues, Scene::mWrappedNoisePeriod divided by this should have no remainder
			mWaveHeightTextureGenerator.reset(new CascadedWaveHeightTextureGenerator(static_cast<const WaveHeightTextureGeneratorFactory&>(*it->second), smallestWaveHeightMapWorldSize));

			for (int i = 0; i < CascadedWaveHeightTextureGenerator::numCascades; ++i)
			{
				stateSetConfig.waveHeightMapWorldSizes[i] = mWaveHeightTextureGenerator->getWorldSize(i);

				stateSetConfig.waveHeightTexture[i] = mWaveHeightTextureGenerator->getTexture(i);
				stateSetConfig.waveHeightTexture[i]->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
				stateSetConfig.waveHeightTexture[i]->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);

				int width = stateSetConfig.waveHeightTexture[i]->getImage(0)->s();
				int height = stateSetConfig.waveHeightTexture[i]->getImage(0)->t();

				osg::Vec2f textureWorldSize(mWaveHeightTextureGenerator->getWorldSize(i), mWaveHeightTextureGenerator->getWorldSize(i));
				const bool generateMipMaps = true;
				stateSetConfig.waveNormalTexture[i] = createNormalTexture(width, height);
				osg::ref_ptr<GpuTextureGenerator> generator = new GpuTextureGenerator(stateSetConfig.waveNormalTexture[i], createVectorDisplacementToNormalMapStateSet(config.programs->getRequiredProgram("vectorDisplacementToNormal"), stateSetConfig.waveHeightTexture[i], textureWorldSize), generateMipMaps);
				mWaterSurfaceGpuTextureGenerators.push_back(generator);
				addTextureGeneratorToSceneGraph(generator);

				WaveFoamMaskGeneratorConfig generatorConfig;
				generatorConfig.program = config.programs->getRequiredProgram("waveFoamMaskGenerator");
				generatorConfig.textureSizePixels = osg::Vec2i(width, height);
				generatorConfig.textureWorldSize = textureWorldSize;
				generatorConfig.waveVectorDisplacementTexture = stateSetConfig.waveHeightTexture[i];
				generatorConfig.parent = mScene->_getGroup();
				mWaveFoamMaskGenerator[i].reset(new WaveFoamMaskGenerator(generatorConfig));
				stateSetConfig.waveFoamMaskTexture[i] = mWaveFoamMaskGenerator[i]->getOutputTexture();


//#define OCEAN_TEXTURE_DEBUG_VIS
#ifdef OCEAN_TEXTURE_DEBUG_VIS
				osg::Vec2f pos(0.2 + 0.4*i, 0.7);
				osg::Vec2f size(0.3, 0.3);
				BoundingBox2f box(pos, pos + size);
				ScreenQuad* quad = new ScreenQuad(createTexturedQuadStateSet(config.programs->hudGeometry, stateSetConfig.waveFoamMaskTexture[i]), box);
				mScene->addObject(quad);
#endif
			}

			mWaterStateSet = new WaterStateSet(stateSetConfig);
		}
		// Create ocean
		OceanConfig oceanConfig;
		if (mWaterStateSet)
		{
			oceanConfig.oceanProgram = config.programs->getRequiredProgram("ocean");
			oceanConfig.waterStateSet = mWaterStateSet;

//#define ENVIRONMENT_MAP_TEXTURE_DEBUG_VIZ
#ifdef ENVIRONMENT_MAP_TEXTURE_DEBUG_VIZ
			osg::Vec2f pos(0.2, 0.7);
			osg::Vec2f size(0.3, 0.3);
			BoundingBox2f box(pos, pos + size);
			ScreenQuad* quad = new ScreenQuad(createTexturedQuadStateSet(config.programs->getRequiredProgram("hudGeometry"), environmentTexture), box);
			mScene->addObject(quad);
#endif

//#define ATMOSPHERIC_SCATTERING_DEBUG_VIZ
#ifdef ATMOSPHERIC_SCATTERING_DEBUG_VIZ
			osg::Vec2f pos(0.2, 0.7);
			osg::Vec2f size(0.3, 0.3);
			BoundingBox2f box(pos, pos + size);
			ScreenQuad* quad = new ScreenQuad(createTexturedQuadStateSet(config.programs->getRequiredProgram("hudGeometry"), mAtmosphere->getTransmittanceTexture()), box);
			mScene->addObject(quad);
#endif

			mOcean.reset(new Ocean(oceanConfig));
			mOcean->setPosition(osg::Vec3f(0, 0, 0));
			mScene->addObject(mOcean.get());
		}
	}

	osg::ref_ptr<osg::Group> nonBuildingsGroup = new osg::Group();
	mTransform->addChild(nonBuildingsGroup);

	osg::ref_ptr<osg::Group> buildingsGroup = new osg::Group();
	mTransform->addChild(buildingsGroup);

	mShadowMapGenerator->registerShadowCaster(buildingsGroup);

	// Features
	if (mWaterStateSet)
	{
		if (!config.featureTreeFiles.empty())
		{
			PlanetFeaturesParams params;
			params.scheduler = config.scheduler;
			params.treeFiles = config.featureTreeFiles;
			params.fileLocator = config.fileLocator;
			params.tilesDirectoryRelAssetPackage = config.featureTilesDirectoryRelAssetPackage;
			params.programs = config.programs;
			params.waterStateSet = mWaterStateSet;
			params.planetRadius = mInnerRadius;
			params.shadowMaps = mShadowMapGenerator->getTextures();

			params.groups[PlanetFeaturesParams::groupsBuildingsIndex] = buildingsGroup;
			params.groups[PlanetFeaturesParams::groupsNonBuildingsIndex] = nonBuildingsGroup;
			mPlanetFeatures.reset(new PlanetFeatures(params));
			
			mPlanetSurface->Listenable<PlanetSurfaceListener>::addListener(mPlanetSurfaceListener.get());
		}
	}

	// Clouds
	if (config.cloudsTexture)
	{
		VolumeCloudsConfig cloudsConfig;
		cloudsConfig.program = config.programs->getRequiredProgram("volumeClouds");
		cloudsConfig.compositorProgram = config.programs->getRequiredProgram("compositeClouds");
		cloudsConfig.innerCloudLayerRadius = config.innerRadius + 3000;
		cloudsConfig.outerCloudLayerRadius = config.innerRadius + 8000;
		cloudsConfig.cloudsTexture = config.cloudsTexture;
		mVolumeClouds.reset(new VolumeClouds(cloudsConfig));

		setCloudsVisible(true);
		setCloudCoverageFraction(boost::none);

		osg::StateSet* ss = mScene->_getGroup()->getOrCreateStateSet();
		ss->setTextureAttributeAndModes((int)GlobalSamplerUnit::GlobalCloudAlpha, config.cloudsTexture);
		ss->addUniform(createUniformSampler2d("cloudSampler", (int)GlobalSamplerUnit::GlobalCloudAlpha));

		{
			osg::ref_ptr<osg::Texture> texture = new osg::Texture2D(osgDB::readImageFile("Environment/Noise.png"));
			texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
			texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
			ss->setTextureAttributeAndModes((int)GlobalSamplerUnit::ShadowCascade0, texture);
			ss->addUniform(createUniformSampler2d("coverageDetailSampler2", (int)GlobalSamplerUnit::ShadowCascade0));
		}

//#define CLOUDS_TEXTURE
#ifdef CLOUDS_TEXTURE
		osg::Vec2f pos(0.2, 0.7);
		osg::Vec2f size(0.3, 0.3);
		BoundingBox2f box(pos, pos + size);
		ScreenQuad* quad = new ScreenQuad(createTexturedQuadStateSet(config.programs->hudGeometry, mVolumeClouds->getColorTexture()), box);
		mScene->addObject(quad);
#endif
	}

	// Shadows
	if (mShadowsEnabled)
	{
		{
			osg::StateSet* ss = mScene->_getGroup()->getOrCreateStateSet();
			ss->setDefine("ENABLE_SHADOWS");
		}

		mShadowSceneTransform->addChild(buildingsGroup);
		for (int i = 0; i < mShadowMapGenerator->getCascadeCount(); ++i)
		{
			mShadowMapGenerator->getCamera(i)->addChild(mShadowSceneTransform);
			mScene->_getGroup()->addChild(mShadowMapGenerator->getCamera(i));
		}

		if (false) // debug shadows
		{
			osg::Vec2f pos(0.2, 0.7);
			osg::Vec2f size(0.3, 0.3);
			BoundingBox2f box(pos, pos + size);
			ScreenQuad* quad = new ScreenQuad(createTexturedQuadStateSet(config.programs->getRequiredProgram("hudGeometry"), mShadowMapGenerator->getTextures()[0]), box);
			mScene->addObject(quad);
		}

		osg::StateSet* ss = mTransform->getOrCreateStateSet();
		mShadowMapGenerator->configureShadowReceiverStateSet(*ss);
	}
}

Planet::~Planet()
{
	if (mShadowMapGenerator)
	{
		for (int i = 0; i < mShadowMapGenerator->getCascadeCount(); ++i)
		{
			mScene->_getGroup()->removeChild(mShadowMapGenerator->getCamera(i));
		}
	}

	removeTextureGeneratorFromSceneGraph(mEnvironmentMapGpuTextureGenerator);
	for (const auto& generator : mWaterSurfaceGpuTextureGenerators)
	{
		removeTextureGeneratorFromSceneGraph(generator);
	}

	if (mOcean)
	{
		mScene->removeObject(mOcean.get());
	}

	if (mAtmosphere)
	{
		mScene->removeObject(mAtmosphere.get());
	}

	if (mPlanetSky)
	{
		mScene->removeObject(mPlanetSky.get());
	}

	setCloudsVisible(false);

	// Remove listener to decouple shutdown order of mPlanetSurface and mPlanetSurfaceListener
	mPlanetSurface->Listenable<PlanetSurfaceListener>::removeListener(mPlanetSurfaceListener.get());
}

void Planet::setCloudsVisible(bool visible)
{
	osg::StateSet* ss = mScene->_getGroup()->getOrCreateStateSet();
	if (visible && !mCloudsVisible)
	{
		mScene->addObject(mVolumeClouds.get());
		ss->setDefine("ENABLE_CLOUDS");
	}
	else if (!visible && mCloudsVisible)
	{
		mScene->removeObject(mVolumeClouds.get());
		ss->removeDefine("ENABLE_CLOUDS");
	}
	mCloudsVisible = visible;
}

void Planet::setCloudCoverageFraction(boost::optional<float> cloudCoverageFraction)
{
	osg::StateSet* ss = mScene->_getGroup()->getOrCreateStateSet();
	if (cloudCoverageFraction)
	{
		mCloudCoverageFractionUniform->set(*cloudCoverageFraction);
		ss->removeDefine("USE_CLOUD_COVERAGE_MAP");
	}
	else
	{
		ss->setDefine("USE_CLOUD_COVERAGE_MAP");
	}
}

float Planet::getWaveHeight() const
{
	if (mWaveHeightTextureGenerator)
		return mWaveHeightTextureGenerator->getWaveHeight();
	return 0.0f;
}

void Planet::setWaveHeight(float height)
{
	if (mWaveHeightTextureGenerator)
		mWaveHeightTextureGenerator->setWaveHeight(height);
}

void Planet::setPosition(const osg::Vec3d &position)
{
	osg::Matrix mat = mTransform->getMatrix();
	mat.setTrans(position);
	mTransform->setMatrix(mat);
}

void Planet::setOrientation(const osg::Quat &orientation)
{
	osg::Matrix mat = mTransform->getMatrix();
	mat.setRotate(orientation);
	mTransform->setMatrix(mat);
}

osg::Node* Planet::_getNode() const
{
	return mTransform.get();
}

void Planet::addTextureGeneratorToSceneGraph(const osg::ref_ptr<GpuTextureGenerator>& generator)
{
	mScene->_getGroup()->addChild(generator);
}

void Planet::removeTextureGeneratorFromSceneGraph(const osg::ref_ptr<GpuTextureGenerator>& generator)
{
	generator->getParent(0)->removeChild(generator);
}

static osg::Quat getOrientationFromAzEl(const AzEl& azEl)
{
	return osg::Quat(azEl.elevation, osg::Vec3f(0, 1, 0)) * osg::Quat(azEl.azimuth, osg::Vec3f(0, 0, 1));
}

void Planet::updatePostSceneUpdate()
{
	osg::Vec3d position = getPosition();
	if (mPlanetSky)
	{
		mPlanetSky->setPosition(osg::Vec3f(position));
	}
	
	if (mOcean)
	{
		mOcean->setPosition(osg::Vec3f(0, 0, position.z() - mInnerRadius));
	}
	
	if (mVolumeClouds)
	{
		mVolumeClouds->setPosition(osg::Vec3f(position));
		mVolumeClouds->setOrientation(getOrientation());
	}
}

void Planet::updatePreRender(const RenderContext& context)
{
	if (mShadowMapGenerator && mShadowsEnabled)
	{
		mShadowSceneTransform->setMatrix(mTransform->getMatrix());
		mShadowMapGenerator->update(context.camera, context.lightDirection, mScene->getWrappedNoiseOrigin());
	}

	osg::Vec3f position = getPosition();
	mPlanetCenterUniform->set(position);

	osg::Matrixf planetMatrix;
	getOrientation().get(planetMatrix);
	planetMatrix = osg::Matrixf::inverse(planetMatrix);
	mPlanetMatrixInvUniform->set(planetMatrix);

	mPlanetSurface->updatePreRender(context);
	if (mPlanetFeatures)
		mPlanetFeatures->updatePreRender(context);

	mEnvironmentMapGpuTextureGenerator->requestRender();

	double julianDateSeconds = mJulianDate * 24.0 * 60.0 * 60.0;
	// Move clouds
	{
		float windSpeed = 10.0;
		osg::Vec2 displacement(0.0, -20000 + std::fmod(julianDateSeconds, 40000) * windSpeed);
		mCloudDisplacementMetersUniform->set(displacement);
	}

	if (mWaveHeightTextureGenerator)
	{
		double loopPeriod = 1000; // TODO: Seamless looping
		double timeSeconds = std::fmod(julianDateSeconds, loopPeriod);
		
		if (mWaveHeightTextureGenerator->update(timeSeconds))
		{
			for (const auto& generator : mWaterSurfaceGpuTextureGenerators)
			{
				generator->requestRender();
			}

			for (int i = 0; i < CascadedWaveHeightTextureGenerator::numCascades; ++i)
			{
				mWaveFoamMaskGenerator[i]->advanceTime(timeSeconds);
				mWaterStateSet->setFoamTexture(i, mWaveFoamMaskGenerator[i]->getOutputTexture());
				mWaveFoamMaskGenerator[i]->swapBuffers();
			}
		}
	}

	if (mReflectionCameraController)
	{
		mReflectionCameraController->update(context.camera);
	}
}

float Planet::calcAtmosphericDensity(const osg::Vec3f& position) const
{
	if (mAtmosphereScaleHeight)
	{
		float altitude = (position - getPosition()).length() - mInnerRadius;
		return 1.225f * std::min(1.0f, std::exp(-altitude / *mAtmosphereScaleHeight));
	}
	return 0.0f;
}

} // namespace vis
} // namespace skybolt
