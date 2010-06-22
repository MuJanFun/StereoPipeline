// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


#include <gtest/gtest.h>

#include <asp/IsisIO/IsisAdjustCameraModel.h>
#include <asp/IsisIO/IsisCameraModel.h>
#include <test/Helpers.h>
#include <boost/foreach.hpp>

using namespace vw;
using namespace vw::camera;
using namespace asp;

class IsisAdjustCameraTest : public ::testing::Test {
protected:
  IsisAdjustCameraTest() {}

  virtual void SetUp() {
    files.push_back("E1701676.reduce.cub");
    files.push_back("5165r.cub");
  }

  Vector2 generate_random( int const& xsize,
                           int const& ysize ) {
    Vector2 pixel;
    pixel[0] = rand() % ( 10 * xsize - 10 ) + 10;
    pixel[0] /= 10;
    pixel[1] = rand() % ( 10 * ysize - 10 ) + 10;
    pixel[1] /= 10;
    return pixel;
  }

  void create_pixels( IsisAdjustCameraModel const& cam ) {
    pixels.clear();
    points.clear();
    for ( unsigned i = 0; i < 100; i++ ) {
      pixels.push_back( generate_random(cam.samples(),cam.lines()) );
      points.push_back( cam.pixel_to_vector(pixels.back()) );
      points.back() *= 100000; // 100 km
      points.back() += cam.camera_center(pixels.back());
    }
  }

  // Feed the camera noise to make sure we are not using stored
  // values.
  void fuzz_camera( IsisAdjustCameraModel const& cam ) {
    Vector2 noise = generate_random( cam.samples(),
                                     cam.lines() );
    Vector3 temp = cam.pixel_to_vector( noise );
  }

  std::vector<Vector2> pixels;
  std::vector<Vector3> points;
  std::vector<std::string> files;
};

TEST_F(IsisAdjustCameraTest, NoFunctions) {
  BOOST_FOREACH( std::string const& cube, files ) {
    boost::shared_ptr<BaseEquation> blank( new PolyEquation(0) );
    IsisAdjustCameraModel cam( cube, blank, blank );
    IsisCameraModel noa_cam( cube );
    create_pixels( cam );
    fuzz_camera( cam );

    for ( unsigned i = 0; i < pixels.size(); i++ ) {
      // Test Position hasn't changed
      fuzz_camera( cam );
      EXPECT_VECTOR_NEAR( cam.camera_center( pixels[i] ),
                          noa_cam.camera_center( pixels[i] ),
                          0.001 );
      EXPECT_MATRIX_NEAR( cam.camera_pose( pixels[i] ).rotation_matrix(),
                          noa_cam.camera_pose( pixels[i] ).rotation_matrix(),
                          0.001 );

      // Test Circle Projection
      Vector2 rpixel = cam.point_to_pixel( points[i] );
      EXPECT_VECTOR_NEAR( pixels[i], rpixel, 0.001 );
    }
  }
}

TEST_F(IsisAdjustCameraTest, PolyFunctions) {
  BOOST_FOREACH( std::string const& cube, files ) {
    boost::shared_ptr<BaseEquation> position( new PolyEquation(1) );
    (*position)[0] = 1000;
    (*position)[1] = 10;
    (*position)[2] = 2000;
    (*position)[3] = -10;
    (*position)[4] = -11000;
    (*position)[5] = 5;
    boost::shared_ptr<BaseEquation> pose( new PolyEquation(0) );
    (*position)[0] = .07;
    (*position)[1] = -.1;
    (*position)[2] = .02;
    IsisAdjustCameraModel cam( cube, position, pose );
    create_pixels( cam );
    fuzz_camera( cam );

    for ( unsigned i = 0; i < pixels.size(); i++ ) {
      Vector2 rpixel = cam.point_to_pixel( points[i] );
      EXPECT_VECTOR_NEAR( pixels[i], rpixel, 0.001 );
      (*position)[2] += 1000;
      Vector3 rpoint = cam.pixel_to_vector( rpixel );
      fuzz_camera(cam);
      rpoint *= 100000;
      rpoint += cam.camera_center( rpixel );
      EXPECT_NEAR( rpoint[0] - points[i][0], 0, .001 );
      EXPECT_NEAR( rpoint[1] - points[i][1], 1000, .001 );
      EXPECT_NEAR( rpoint[2] - points[i][2], 0, .001 );
      (*position)[2] -= 1000;
    }
  }
}

TEST_F(IsisAdjustCameraTest, RPNFunctions) {
  BOOST_FOREACH( std::string const& cube, files ) {
    std::string xpos_eq = "t 2 * 100 / 99 +";
    std::string ypos_eq = "t .8 * 1000 -";
    std::string zpos_eq = "t .5 * 2000 +";
    std::string xang_eq = ".005";
    std::string yang_eq = "-.013 t *";
    std::string zang_eq = "0";
    boost::shared_ptr<BaseEquation> position( new RPNEquation( xpos_eq,
                                                               ypos_eq,
                                                               zpos_eq ) );
    boost::shared_ptr<BaseEquation> pose( new RPNEquation( xang_eq,
                                                           yang_eq,
                                                           zang_eq ) );
    IsisAdjustCameraModel cam( cube, position, pose );
    create_pixels( cam );
    fuzz_camera( cam );

    for ( unsigned i = 0; i < pixels.size(); i++ ) {
      Vector2 rpixel = cam.point_to_pixel( points[i] );
      EXPECT_VECTOR_NEAR( pixels[i], rpixel, 0.001 );
      (*position)[2] -= 500;
      Vector3 rpoint = cam.pixel_to_vector( rpixel );
      fuzz_camera( cam );
      rpoint *= 100000;
      rpoint += cam.camera_center( rpixel );
      EXPECT_NEAR( rpoint[0] - points[i][0], -500, .001 );
      EXPECT_NEAR( rpoint[1] - points[i][1], 0, .001 );
      EXPECT_NEAR( rpoint[2] - points[i][2], 0, .001 );
      (*position)[2] += 500;
    }
  }
}
