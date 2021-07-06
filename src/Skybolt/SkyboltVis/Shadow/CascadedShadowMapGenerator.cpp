#include "CascadedShadowMapGenerator.h"
#include "Camera.h"
#include "ShadowMapGenerator.h"
#include <assert.h>

namespace skybolt {
namespace vis {

static std::vector<float> calculateSplitDistances(size_t cascadeCount, float firstSplitDistance, float farDistance, float lambda = 0.5)
{
	assert(cascadeCount >= 1);

	std::vector<float> splitDistances(cascadeCount - 1);
	splitDistances[0] = firstSplitDistance;

	for (size_t i = 1; i < splitDistances.size(); i++)
	{
		float fraction = float(i + 1) / cascadeCount;
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

void CascadedShadowMapGenerator::update(const vis::Camera& viewCamera, const osg::Vec3& lightDirection, const osg::Vec3& wrappedNoiseOrigin)
{
	calculateSplitDistances(3, 2000, 100000);

	std::vector<float> cascadeBoundingDistances = {0, 2000, 10000, 50000};

	for (size_t i = 0; i < mShadowMapGenerators.size(); ++i)
	{
		const auto& generator = mShadowMapGenerators[i];

		Frustum frustum;
		frustum.fieldOfViewY = viewCamera.getFovY();
		frustum.aspectRatio = viewCamera.getAspectRatio();
		frustum.nearPlaneDistance = cascadeBoundingDistances[i];
		frustum.farPlaneDistance = cascadeBoundingDistances[i + 1];
		auto result = calculateMinimalEnclosingSphere(frustum);

		generator->setRadiusWorldSpace(result.radius);

		osg::Vec3 viewCameraDirection = viewCamera.getOrientation() * osg::Vec3f(1, 0, 0);
		osg::Vec3 shadowCameraPosition = viewCamera.getPosition() + viewCameraDirection * result.centerDistance + lightDirection * 5000;
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

	return osg::Vec4f(
		scale,
		offsetX,
		offsetY,
		offsetZ
	);
}

CascadedShadowMapGenerator::FrustumMinimalEnclosingSphere CascadedShadowMapGenerator::calculateMinimalEnclosingSphere(const Frustum& frustum)
{
	// Technique from https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
	float k = std::sqrt(1 + frustum.aspectRatio * frustum.aspectRatio) * std::tan(frustum.fieldOfViewY / 2.f);
	float k2 = k * k;
	float k4 = k2 * k2;

	if (k*k >= (frustum.farPlaneDistance - frustum.nearPlaneDistance) / (frustum.farPlaneDistance + frustum.nearPlaneDistance))
	{
		return { frustum.farPlaneDistance, frustum.farPlaneDistance * k };
	}
	else
	{
		float fPlusN = frustum.farPlaneDistance + frustum.nearPlaneDistance;
		float centerDistance = 0.5f * fPlusN * (1 + k2);
		float radius = 0.5f * std::sqrt(
			osg::square(frustum.farPlaneDistance - frustum.nearPlaneDistance)
			+ 2 * ( osg::square(frustum.farPlaneDistance) + osg::square(frustum.nearPlaneDistance)) * k2
			+ osg::square(fPlusN) * k4
		);
		return { centerDistance, radius };
	}
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
