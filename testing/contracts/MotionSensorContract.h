#pragma once

// The contract of MotionSensor. Rig: `MotionSensor& sensor()`,
//   bool setAvailable(bool)     make the sensor present or absent; false if not possible
//   bool failNextStart()        the next start() fails; false if not possible
//   bool tilt(x, y, z)          what it reads while at rest; false if not possible

#include <cmath>

#include <hal/MotionSensor.h>

#include "Contract.h"

template <class Rig>
struct MotionSensorContract {
  static void it_is_not_running_before_it_is_started() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.sensor().running());
    MotionSample s;
    TEST_ASSERT_FALSE(rig.sensor().read(s));
  }

  static void started_it_runs_and_gives_a_value() {
    Rig rig;
    CONTRACT_NEEDS(rig.sensor().available());
    TEST_ASSERT_TRUE(rig.sensor().start());
    TEST_ASSERT_TRUE(rig.sensor().running());
    MotionSample s;
    TEST_ASSERT_TRUE(rig.sensor().read(s));
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_TRUE(std::isfinite(s.ax) && std::isfinite(s.ay) && std::isfinite(s.az));
  }

  // Lying flat reads about +1 g on z: the unit is g, not m/s^2.
  static void a_device_at_rest_reads_one_g() {
    Rig rig;
    CONTRACT_NEEDS(rig.tilt(0, 0, 1));
    rig.sensor().start();
    MotionSample s;
    TEST_ASSERT_TRUE(rig.sensor().read(s));
    const float g = std::sqrt(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
    TEST_ASSERT_TRUE(g > 0.9f && g < 1.1f);
  }

  static void stopped_it_gives_no_value_and_leaves_the_sample_alone() {
    Rig rig;
    rig.sensor().start();
    rig.sensor().stop();
    TEST_ASSERT_FALSE(rig.sensor().running());
    MotionSample s;
    s.ax = 7.0f;
    TEST_ASSERT_FALSE(rig.sensor().read(s));
    TEST_ASSERT_TRUE(s.ax == 7.0f);
  }

  static void starting_and_stopping_twice_is_harmless() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.sensor().start());
    TEST_ASSERT_TRUE(rig.sensor().start());
    TEST_ASSERT_TRUE(rig.sensor().running());
    rig.sensor().stop();
    rig.sensor().stop();
    TEST_ASSERT_FALSE(rig.sensor().running());
    TEST_ASSERT_TRUE(rig.sensor().start());
  }

  static void a_sensor_that_is_not_there_cannot_be_started() {
    Rig rig;
    CONTRACT_NEEDS(rig.setAvailable(false));
    TEST_ASSERT_FALSE(rig.sensor().available());
    TEST_ASSERT_FALSE(rig.sensor().start());
    TEST_ASSERT_FALSE(rig.sensor().running());
  }

  static void a_start_that_fails_leaves_it_not_running() {
    Rig rig;
    CONTRACT_NEEDS(rig.failNextStart());
    TEST_ASSERT_FALSE(rig.sensor().start());
    TEST_ASSERT_FALSE(rig.sensor().running());
    MotionSample s;
    TEST_ASSERT_FALSE(rig.sensor().read(s));
  }
};

#define RUN_MOTION_SENSOR_CONTRACT(Rig)                                                   \
  RUN_TEST(MotionSensorContract<Rig>::it_is_not_running_before_it_is_started);             \
  RUN_TEST(MotionSensorContract<Rig>::started_it_runs_and_gives_a_value);                  \
  RUN_TEST(MotionSensorContract<Rig>::a_device_at_rest_reads_one_g);                       \
  RUN_TEST(MotionSensorContract<Rig>::stopped_it_gives_no_value_and_leaves_the_sample_alone); \
  RUN_TEST(MotionSensorContract<Rig>::starting_and_stopping_twice_is_harmless);            \
  RUN_TEST(MotionSensorContract<Rig>::a_sensor_that_is_not_there_cannot_be_started);       \
  RUN_TEST(MotionSensorContract<Rig>::a_start_that_fails_leaves_it_not_running)
