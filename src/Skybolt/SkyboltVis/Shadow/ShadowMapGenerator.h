#pragma once

#include <osg/Camera>
#include <osg/Group>
#include <osg/Program>
#include <osg/Texture2D>

namespace skybolt {
namespace vis {

class ShadowMapGenerator
{
public:
	ShadowMapGenerator(osg::ref_ptr<osg::Program> shadowCasterProgram, int shadowMapId = 0);

	void update(const osg::Vec3& shadowCameraPosition, const osg::Vec3& lightDirection, const osg::Vec3& wrappedNoiseOrigin);
	double getRadiusWorldSpace() const { return mRadiusWorldSpace; }
	void setRadiusWorldSpace(double radius);

	void configureShadowReceiverStateSet(osg::StateSet& ss);

	void registerShadowCaster(const osg::ref_ptr<osg::Group>& object);

	osg::ref_ptr<osg::Texture2D> getTexture() const { return mTexture; }
	osg::ref_ptr<osg::Camera> getCamera() const { return mCamera; }

	osg::Matrix getShadowProjectionMatrix() const;

private:
	osg::ref_ptr<osg::Texture2D> mTexture;
	osg::ref_ptr<osg::Camera> mCamera;
	osg::ref_ptr<osg::Uniform> mCameraPositionUniform;
	osg::ref_ptr<osg::Uniform> mViewMatrixUniform;
	osg::ref_ptr<osg::Uniform> mViewProjectionMatrixUniform;
	osg::ref_ptr<osg::Uniform> mShadowProjectionMatrixUniform;
	float mRadiusWorldSpace = 2000;

	std::vector<osg::ref_ptr<osg::Group>> mShadowCasters;
};

} // namespace vis
} // namespace skybolt
