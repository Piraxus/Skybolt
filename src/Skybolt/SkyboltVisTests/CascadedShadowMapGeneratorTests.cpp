/* Copyright 2012-2020 Matthew Reid
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */


#include <catch2/catch.hpp>
#include <SkyboltCommon/NumericComparison.h>
#include <SkyboltVis/Shadow/CascadedShadowMapGenerator.h>

using namespace skybolt::vis;
using namespace osg;

static osg::Vec3 applyTransform(const osg::Vec3& p, const osg::Vec4& transform)
{
	osg::Vec3 q = p;
	q.x() = q.x() * transform.x() + transform.y();
	q.y() = q.y() * transform.x() + transform.z();
	q.z() = q.z() + transform.w();
	return q;
}

// Tests calculation of the linear transform to transform a point in one shadow cascade's coordinate system
// to another shadow cascade's coordinate system.
TEST_CASE("Calculate cascade to cascade transform")
{
	float width0 = 10;
	osg::Matrix m0 = osg::Matrix::identity();
	m0.preMultScale(osg::Vec3f(width0, width0, 1));
	m0.setTrans(1, 2, 3);

	float width1 = 20;
	osg::Matrix m1 = osg::Matrix::identity();
	m1.preMultScale(osg::Vec3f(width1, width1, 1));
	m1.setTrans(4, 5, 6);

	osg::Vec4 transform = CascadedShadowMapGenerator::calculateCascadeToCascadeTransform(m0, m1);

	osg::Vec3 p(5, 10, 15);
	osg::Vec3 q0 = p * m0;
	osg::Vec3 q1Expected = p * m1;

	osg::Vec3 q1 = applyTransform(q0, transform);

	float eps = 1e-8f;
	CHECK(q1Expected.x() == Approx(q1.x()).margin(eps));
	CHECK(q1Expected.y() == Approx(q1.y()).margin(eps));
	CHECK(q1Expected.z() == Approx(q1.z()).margin(eps));
}

TEST_CASE("Calculate minimal enclosing sphere of frustum")
{
	SECTION("When field of view is wide, the sphere exactly encompases the far plane") {
		CascadedShadowMapGenerator::Frustum frustum;
		frustum.aspectRatio = 2.0;
		frustum.farPlaneDistance = 100;
		frustum.nearPlaneDistance = 10;
		frustum.fieldOfViewY = osg::DegreesToRadians(90.f);

		CascadedShadowMapGenerator::FrustumMinimalEnclosingSphere result = CascadedShadowMapGenerator::calculateMinimalEnclosingSphere(frustum);

		float eps = 1e-8f;
		CHECK(100 == Approx(result.centerDistance).margin(eps));
		CHECK(2.f * osg::Vec2(100, 50).length() == Approx(result.radius).margin(eps));
	}

	SECTION("Wide field of view is 0, the sphere exactly encompases the center view vector from near to far plane") {
		CascadedShadowMapGenerator::Frustum frustum;
		frustum.aspectRatio = 2.0;
		frustum.farPlaneDistance = 100;
		frustum.nearPlaneDistance = 10;
		frustum.fieldOfViewY = 0.f;

		CascadedShadowMapGenerator::FrustumMinimalEnclosingSphere result = CascadedShadowMapGenerator::calculateMinimalEnclosingSphere(frustum);

		float eps = 1e-8f;
		CHECK(55 == Approx(result.centerDistance).margin(eps));
		CHECK(45 == Approx(result.radius).margin(eps));
	}
}
