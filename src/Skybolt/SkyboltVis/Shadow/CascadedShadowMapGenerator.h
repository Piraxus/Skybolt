#pragma once

#include <osg/Group>
#include <osg/Program>
#include <osg/Texture2D>

namespace skybolt {
namespace vis {

class CascadedShadowMapGenerator
{
public:
	CascadedShadowMapGenerator(osg::ref_ptr<osg::Program> shadowCasterProgram, int cascadeCount);

	void update(const osg::Vec3& viewCameraPosition, const osg::Vec3& viewCameraDirection, const osg::Vec3& lightDirection, const osg::Vec3& wrappedNoiseOrigin);

	void configureShadowReceiverStateSet(osg::StateSet& ss);

	void registerShadowCaster(const osg::ref_ptr<osg::Group>& object);

	std::vector<osg::ref_ptr<osg::Texture2D>> getTextures() const { return mTextures; }

	osg::ref_ptr<osg::Camera> getCamera(int cascadeIndex) const;

	int getCascadeCount() const { return int(mShadowMapGenerators.size()); }

	static osg::Vec4 calculateCascadeToCascadeTransform(const osg::Matrix m0, const osg::Matrix m1);

private:
	osg::Vec4f calculateCascadeShadowMatrixModifier(int i) const;

private:
	std::vector<osg::ref_ptr<osg::Texture2D>> mTextures;
	std::vector<std::shared_ptr<class ShadowMapGenerator>> mShadowMapGenerators;

	//! Uniforms of Vec4f for each cascade, storing [scaleXY, offsetX, offsetY, offsetZ] relative to the first cascade.
	//! This is used by the shader to avoid having multiplying the shaded point by the matrix of every cascade.
	//! Instead, the higher cascades transformation is derived from the first cascade's matrix multiplied result.
	std::vector<osg::ref_ptr<osg::Uniform>> mCascadeShadowMatrixModifierUniform;
};

} // namespace vis
} // namespace skybolt
