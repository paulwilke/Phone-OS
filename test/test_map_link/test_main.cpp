// Host tests for the map link.
//
// The template is a setting because two obvious choices failed on a real device:
// an OpenStreetMap link never arrived (German networks filter links in SMS from
// foreign numbers) and a geo: address arrived but could not be opened. Which
// address works only the person who knows the target phone can say.

#include <unity.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#include <app/MapLink.h>

using maplink::Result;

void setUp() {}
void tearDown() {}

namespace {

std::string degrees(double v) {
  char out[24];
  TEST_ASSERT_TRUE(maplink::formatDegrees(v, out, sizeof(out)));
  return out;
}

}  // namespace

void test_the_default_is_google_maps() {
  char out[96];
  TEST_ASSERT_TRUE(maplink::build(maplink::defaultTemplate(), 48.137452, 11.575500, out, sizeof(out)) == Result::Ok);
  TEST_ASSERT_EQUAL_STRING("https://maps.google.com/?q=48.13745,11.57550", out);
}

void test_other_services_work_too() {
  char out[96];
  maplink::build("https://waze.com/ul?ll={lat},{lon}", 48.1, 11.5, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("https://waze.com/ul?ll=48.10000,11.50000", out);

  maplink::build("https://www.openstreetmap.org/?mlat={lat}&mlon={lon}", 48.1, 11.5, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("https://www.openstreetmap.org/?mlat=48.10000&mlon=11.50000", out);

  maplink::build("geo:{lat},{lon}", 48.1, 11.5, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("geo:48.10000,11.50000", out);
}

// An empty template means no address at all, not the default: whoever clears the
// setting does not want a link.
void test_an_empty_template_yields_nothing() {
  char out[96] = "leftover";
  TEST_ASSERT_TRUE(maplink::build("", 48.1, 11.5, out, sizeof(out)) == Result::NoTemplate);
  TEST_ASSERT_EQUAL_STRING("", out);
  strcpy(out, "leftover");
  TEST_ASSERT_TRUE(maplink::build(nullptr, 48.1, 11.5, out, sizeof(out)) == Result::NoTemplate);
  TEST_ASSERT_EQUAL_STRING("", out);
}

// South and west are negative; the sign is where a hand-made number output usually goes wrong.
void test_negative_coordinates() {
  char out[96];
  maplink::build("{lat},{lon}", -48.13745, -11.57550, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("-48.13745,-11.57550", out);
  maplink::build("{lat}", 0.0, 0.0, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("0.00000", out);
}

// A tiny negative value that rounds to zero must not print as "-0.00000".
void test_a_value_that_rounds_to_zero_has_no_minus_sign() {
  TEST_ASSERT_EQUAL_STRING("0.00000", degrees(-0.000001).c_str());
  TEST_ASSERT_EQUAL_STRING("0.00000", degrees(-0.000004).c_str());
  TEST_ASSERT_EQUAL_STRING("0.00000", degrees(-0.0).c_str());
  TEST_ASSERT_EQUAL_STRING("-0.00001", degrees(-0.000011).c_str());
}

void test_rounding_pads_and_carries() {
  TEST_ASSERT_EQUAL_STRING("0.00001", degrees(0.00001).c_str());   // leading zeros of the fraction
  TEST_ASSERT_EQUAL_STRING("48.13746", degrees(48.137456).c_str());
  TEST_ASSERT_EQUAL_STRING("48.13745", degrees(48.137454).c_str());
  TEST_ASSERT_EQUAL_STRING("2.00000", degrees(1.999999).c_str());  // carry into the integer part
  TEST_ASSERT_EQUAL_STRING("11.57550", degrees(11.5755).c_str());
  TEST_ASSERT_EQUAL_STRING("179.99999", degrees(179.999994).c_str());
}

void test_degrees_reject_what_is_not_a_number() {
  char out[24] = "x";
  TEST_ASSERT_FALSE(maplink::formatDegrees(std::numeric_limits<double>::quiet_NaN(), out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
  TEST_ASSERT_FALSE(maplink::formatDegrees(std::numeric_limits<double>::infinity(), out, sizeof(out)));
  TEST_ASSERT_FALSE(maplink::formatDegrees(1e12, out, sizeof(out)));
  TEST_ASSERT_FALSE(maplink::formatDegrees(1.0, out, 7));  // "1.00000" needs 8 bytes with the terminator
  TEST_ASSERT_TRUE(maplink::formatDegrees(1.0, out, 8));
  TEST_ASSERT_FALSE(maplink::formatDegrees(1.0, nullptr, 9));
  TEST_ASSERT_FALSE(maplink::formatDegrees(1.0, out, 0));
}

// A position outside the globe is a receiver without a fix or garbage; a link to it
// would be worse than none.
void test_invalid_coordinates_give_no_link() {
  char out[96] = "leftover";
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const double cases[][2] = {{nan, 11.5}, {48.1, nan}, {inf, 11.5}, {48.1, -inf}, {90.1, 11.5},
                             {-90.1, 11.5}, {48.1, 180.1}, {48.1, -181.0}};
  for (const auto& c : cases) {
    strcpy(out, "leftover");
    TEST_ASSERT_TRUE(maplink::build(maplink::defaultTemplate(), c[0], c[1], out, sizeof(out)) == Result::InvalidCoordinates);
    TEST_ASSERT_EQUAL_STRING("", out);
  }
}

void test_the_edges_of_the_globe_are_valid() {
  char out[96];
  TEST_ASSERT_TRUE(maplink::build("{lat}|{lon}", 90.0, 180.0, out, sizeof(out)) == Result::Ok);
  TEST_ASSERT_EQUAL_STRING("90.00000|180.00000", out);
  TEST_ASSERT_TRUE(maplink::build("{lat}|{lon}", -90.0, -180.0, out, sizeof(out)) == Result::Ok);
  TEST_ASSERT_EQUAL_STRING("-90.00000|-180.00000", out);
}

// A typo in the template stays visible instead of silently disappearing.
void test_a_typo_stays_visible() {
  char out[96];
  maplink::build("https://map.example/?p={latt},{lon}", 48.1, 11.5, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("https://map.example/?p={latt},11.50000", out);
  maplink::build("{lat", 48.1, 11.5, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("{lat", out);
  maplink::build("{LAT}", 48.1, 11.5, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("{LAT}", out);
}

void test_placeholders_may_repeat_and_come_in_any_order() {
  char out[96];
  maplink::build("{lon}|{lat}{lat}", 48.1, 11.5, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("11.50000|48.1000048.10000", out);
  maplink::build("https://map.example/", 48.1, 11.5, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("https://map.example/", out);
}

// A link cut short would point to another place. Nothing is better than that.
void test_a_link_that_does_not_fit_is_not_cut_short() {
  char out[12];
  memset(out, 'x', sizeof(out));
  TEST_ASSERT_TRUE(maplink::build(maplink::defaultTemplate(), 48.1, 11.5, out, sizeof(out)) == Result::BufferTooSmall);
  TEST_ASSERT_EQUAL_STRING("", out);

  // "geo:48.10000,11.50000" is 21 characters: 22 bytes fit, 21 do not.
  char exact[22], tight[21];
  TEST_ASSERT_TRUE(maplink::build("geo:{lat},{lon}", 48.1, 11.5, exact, sizeof(exact)) == Result::Ok);
  TEST_ASSERT_EQUAL_STRING("geo:48.10000,11.50000", exact);
  TEST_ASSERT_TRUE(maplink::build("geo:{lat},{lon}", 48.1, 11.5, tight, sizeof(tight)) == Result::BufferTooSmall);
  TEST_ASSERT_EQUAL_STRING("", tight);

  TEST_ASSERT_TRUE(maplink::build("geo:{lat}", 48.1, 11.5, nullptr, 0) == Result::BufferTooSmall);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_the_default_is_google_maps);
  RUN_TEST(test_other_services_work_too);
  RUN_TEST(test_an_empty_template_yields_nothing);
  RUN_TEST(test_negative_coordinates);
  RUN_TEST(test_a_value_that_rounds_to_zero_has_no_minus_sign);
  RUN_TEST(test_rounding_pads_and_carries);
  RUN_TEST(test_degrees_reject_what_is_not_a_number);
  RUN_TEST(test_invalid_coordinates_give_no_link);
  RUN_TEST(test_the_edges_of_the_globe_are_valid);
  RUN_TEST(test_a_typo_stays_visible);
  RUN_TEST(test_placeholders_may_repeat_and_come_in_any_order);
  RUN_TEST(test_a_link_that_does_not_fit_is_not_cut_short);
  return UNITY_END();
}
