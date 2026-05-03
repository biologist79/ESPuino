/* --- GENERATED VOLUME LUT HEADER --- */
#include <pgmspace.h>

static constexpr int VOL_LUT_STEPS = 32;
static constexpr int VOL_LUT_CURVES = 2;

// Curve Indices
static constexpr int VOL_CURVE_SQUARED = 0;
static constexpr int VOL_CURVE_PERCEPTUAL = 1;

const float VOLUME_TABLE[VOL_LUT_CURVES][VOL_LUT_STEPS] PROGMEM = {
	{// SQUARED
-60.00f, -53.81f, -45.75f, -39.70f, -35.07f, -31.38f, -28.31f, -25.69f,
		-23.41f, -21.39f, -19.58f, -17.94f, -16.44f, -15.06f, -13.78f, -12.58f,
		-11.47f, -10.42f,  -9.43f,  -8.49f,  -7.60f,  -6.76f, -5.95f, -5.18f,
		-4.44f, -3.73f, -3.05f, -2.40f, -1.77f, -1.16f, -0.57f, 0.00f},
	{// PERCEPTUAL
-60.00f, -59.71f, -57.94f, -54.40f, -50.04f, -45.69f, -41.68f, -38.06f,
		-34.81f, -31.89f, -29.24f, -26.81f, -24.59f, -22.54f, -20.63f, -18.85f,
		-17.18f, -15.61f, -14.13f, -12.73f, -11.40f, -10.13f, -8.92f, -7.77f,
		-6.66f, -5.60f, -4.58f, -3.60f, -2.65f, -1.74f, -0.85f, 0.00f}
};
