#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "cpu_accel.h"

#include "mmxsse_motion.h"
#include "mjpeg_logging.h"

void enable_mmxsse_motion(int cpucap)
{
	if(cpucap & ACCEL_X86_MMXEXT ) /* AMD MMX or SSE... */
	{
		mjpeg_info( "SETTING EXTENDED MMX for MOTION!");
		if (disable_simd("sad_sub22") != 0)
		   mjpeg_info(" Disabling sad_sub22");
		else
		   psad_sub22 = sad_sub22_mmxe;

		if (disable_simd("sad_sub44") != 0)
		   mjpeg_info(" Disabling sad_sub44");
		else
		   psad_sub44 = sad_sub44_mmxe;

		if (disable_simd("sad_00") != 0)
		   mjpeg_info(" Disabling sad_00");
		else
		   psad_00 = sad_00_mmxe;

		if (disable_simd("sad_01") != 0)
		   mjpeg_info(" Disabling sad_01");
		else
		   psad_01 = sad_01_mmxe;

		if (disable_simd("sad_10") != 0)
		   mjpeg_info(" Disabling sad_10");
		else
		   psad_10 = sad_10_mmxe;

		if (disable_simd("sad_11") != 0)
		   mjpeg_info(" Disabling sad_11");
		else
		   psad_11 = sad_11_mmxe;

		if (disable_simd("bsad") != 0)
		   mjpeg_info(" Disabling bsad");
		else
		   pbsad = bsad_mmx;

		if (disable_simd("variance") != 0)
		   mjpeg_info(" Disabling variance");
		else
		   pvariance = variance_mmx;

		if (disable_simd("sumsq") != 0)
		   mjpeg_info(" Disabling sumsq");
		else
		   psumsq = sumsq_mmx;

		if (disable_simd("bsumsq") != 0)
		   mjpeg_info(" Disabling bsumsq");
		else
		   pbsumsq = bsumsq_mmx;

		if (disable_simd("sumsq_sub22") != 0)
		   mjpeg_info(" Disabling sumsq_sub22");
		else
		   psumsq_sub22 = sumsq_sub22_mmx;

		if (disable_simd("bsumsq_sub22") != 0)
		   mjpeg_info(" Disabling bsumsq_sub22");
		else
		   pbsumsq_sub22 = bsumsq_sub22_mmx;

		if (disable_simd("find_best_one_pel") != 0)
		   mjpeg_info(" Disabling find_best_one_pel");
		else
		   pfind_best_one_pel = find_best_one_pel_mmxe;

		if (disable_simd("build_sub22_mests") != 0)
		   mjpeg_info(" Disabling build_sub22_mests");
		else
		   pbuild_sub22_mests	= build_sub22_mests_mmxe;

		if (disable_simd("build_sub44_mests") != 0)
		   mjpeg_info(" Disabling build_sub44_mests");
		else
		   pbuild_sub44_mests	= build_sub44_mests_mmx;

		if (disable_simd("mblocks_sub44_mests") != 0)
		   mjpeg_info(" Disabling mblocks_sub44_mests");
		else
		   pmblocks_sub44_mests = mblocks_sub44_mests_mmxe;
	}
	else if(cpucap & ACCEL_X86_MMX) /* Ordinary MMX CPU */
	{
		mjpeg_info( "SETTING MMX for MOTION!");
		if (disable_simd("sad_sub22") != 0)
		   mjpeg_info(" Disabling sad_sub22");
		else
		   psad_sub22 = sad_sub22_mmx;

		if (disable_simd("sad_sub44") != 0)
		   mjpeg_info(" Disabling sad_sub44");
		else
		   psad_sub44 = sad_sub44_mmx;

		if (disable_simd("sad_00") != 0)
		   mjpeg_info(" Disabling sad_00");
		else
		   psad_00 = sad_00_mmx;

		if (disable_simd("sad_01") != 0)
		   mjpeg_info(" Disabling sad_01");
		else
		   psad_01 = sad_01_mmx;

		if (disable_simd("sad_10") != 0)
		   mjpeg_info(" Disabling sad_10");
		else
		   psad_10 = sad_10_mmx;

		if (disable_simd("sad_11") != 0)
		   mjpeg_info(" Disabling sad_11");
		else
		   psad_11 = sad_11_mmx;

		if (disable_simd("bsad") != 0)
		   mjpeg_info(" Disabling bsad");
		else
		   pbsad = bsad_mmx;

		if (disable_simd("variance") != 0)
		   mjpeg_info(" Disabling variance");
		else
		   pvariance = variance_mmx;

		if (disable_simd("sumsq") != 0)
		   mjpeg_info(" Disabling sumsq");
		else
		   psumsq = sumsq_mmx;

		if (disable_simd("bsumsq") != 0)
		   mjpeg_info(" Disabling bsumsq");
		else
		   pbsumsq = bsumsq_mmx;

		if (disable_simd("sumsq_sub22") != 0)
		   mjpeg_info(" Disabling sumsq_sub22");
		else
		   psumsq_sub22 = sumsq_sub22_mmx;

		if (disable_simd("bsumsq_sub22") != 0)
		   mjpeg_info(" Disabling bsumsq_sub22");
		else
		   pbsumsq_sub22 = bsumsq_sub22_mmx;

		// NOT CHANGED pfind_best_one_pel;
		// NOT CHANGED pbuild_sub22_mests	= build_sub22_mests;

		if (disable_simd("build_sub44_mests") != 0)
		   mjpeg_info(" Disabling build_sub44_mests");
		else
		   pbuild_sub44_mests	= build_sub44_mests_mmx;

		if (disable_simd("mblocks_sub44_mests") != 0)
		   mjpeg_info(" Disabling mblocks_sub44_mests");
		else
		   pmblocks_sub44_mests = mblocks_sub44_mests_mmx;
	}
}
