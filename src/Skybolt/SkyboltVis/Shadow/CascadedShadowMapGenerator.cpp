#include "CascadedShadowMapGenerator.h"
#include "ShadowMapGenerator.h"
#include <assert.h>

namespace skybolt {
namespace vis {

static std::vector<float> calculateSplitDistances(size_t cascadeCount, float firstSplitDistance, float farDistance, float lambda = 0.5)
{
	assert(cascadeCount >= 1);

	std::vector<float> splitDistances(cascadeCount-1);
	splitDistances[0] = firstSplitDistance;

	for (size_t i = 1; i < splitDistances.size(); i++)
	{
		float fraction = float(i+1) / cascadeCount;
		float logDistance = firstSplitDistance * std::pow(farDistance / firstSplitDistance, fraction);
		float linDistance = firstSplitDistance + (farDistance - firstSplitDistance) * fraction;
		float splitDistance = linDistance + lambda * (logDistance - linDistance);

		splitDistances[i] = splitDistance;
	}
	return splitDistances;
}

CascadedShadowMapGenerator::CascadedShadowMapGenerator(osg::ref_ptr<osg::Program> shadowCasterProgram, int cascadeCount)
{
	assert(cascadeCount >= 1);
	for (int i = 0; i < cascadeCount; ++i)
	{
		mShadowMapGenerators.push_back(std::make_shared<ShadowMapGenerator>(shadowCasterProgram, i));
		mTextures.push_back(mShadowMapGenerators[i]->getTexture());

		mCascadeShadowMatrixModifierUniform.push_back(new osg::Uniform(std::string("cascadeShadowMatrixModifier" + std::to_string(i)).c_str(), osg::Vec4f(1, 0, 0, 1)));
	}
}

void CascadedShadowMapGenerator::update(const osg::Vec3& viewCameraPosition, const osg::Vec3& viewCameraDirection, const osg::Vec3& lightDirection, const osg::Vec3& wrappedNoiseOrigin)
{
	calculateSplitDistances(3, 2000, 100000);

	std::vector<float> cascadeBoundingDistances = {0, 2000, 10000, 50000};

	for (size_t i = 0; i < mShadowMapGenerators.size(); ++i)
	{
		const auto& generator = mShadowMapGenerators[i];
		
		double cascadeSize = cascadeBoundingDistances[i+1] - cascadeBoundingDistances[i];
		generator->setRadiusWorldSpace(cascadeSize * 0.5);

		double cascadeCenterDistance = (cascadeBoundingDistances[i] + cascadeBoundingDistances[i+1]) * 0.5f;

		osg::Vec3 shadowCameraPosition = viewCameraPosition + viewCameraDirection * cascadeCenterDistance + lightDirection * 5000;
		generator->update(shadowCameraPosition, lightDirection, wrappedNoiseOrigin);
	}

	
	for (size_t i = 0; i < mCascadeShadowMatrixModifierUniform.size(); ++i)
	{
		mCascadeShadowMatrixModifierUniform[i]->set(calculateCascadeShadowMatrixModifier(i));
	}
}

osg::Vec4 CascadedShadowMapGenerator::calculateCascadeToCascadeTransform(const osg::Matrix m0, const osg::Matrix m1)
{
	float scale = m1.getScale().x() / m0.getScale().x();

	float offsetX = m1(3, 0) - m0(3, 0) * scale;
	float offsetY = m1(3, 1) - m0(3, 1) * scale;
	float offsetZ = m1(3, 2) - m0(3, 2);

	//float oneOnWidth = 1.0f / width0;
	//float offCenter = 0.5 - 0.5 * width0 / width1;

	return osg::Vec4f(
		scale,
		offsetX,
		offsetY,
		offsetZ
	);
}

osg::Vec4f CascadedShadowMapGenerator::calculateCascadeShadowMatrixModifier(int i) const
{
	osg::Matrix mat0 = mShadowMapGenerators[0]->getShadowProjectionMatrix();
	osg::Matrix mat = mShadowMapGenerators[i]->getShadowProjectionMatrix();

	return calculateCascadeToCascadeTransform(mat0, mat);
}

void CascadedShadowMapGenerator::configureShadowReceiverStateSet(osg::StateSet& ss)
{
	for (const auto& generator : mShadowMapGenerators)
	{
		generator->configureShadowReceiverStateSet(ss);
	}

	for (const auto& uniform : mCascadeShadowMatrixModifierUniform)
	{
		ss.addUniform(uniform);
	}
}

void CascadedShadowMapGenerator::registerShadowCaster(const osg::ref_ptr<osg::Group>& object)
{
	for (const auto& generator : mShadowMapGenerators)
	{
		generator->registerShadowCaster(object);
	}
}

osg::ref_ptr<osg::Camera> CascadedShadowMapGenerator::getCamera(int cascadeIndex) const
{
	return mShadowMapGenerators[cascadeIndex]->getCamera();
}

} // namespace vis
} // namespace skybolt
