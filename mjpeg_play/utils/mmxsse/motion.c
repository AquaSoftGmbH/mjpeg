#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "cpu_accel.h"

#include "mmxsse_motion.h"
#include "../mjpeg_logging.h"

void enable_mmxsse_motion(int cpucap)
{
	if(cpucap & ACCEL_X86_MMXEXT ) /* AMD MMX or SSE... */
	{
		mjpeg_info( "SETTING EXTENDED MMX for MOTION!");
		psad_sub22 = sad_sub22_mmxe;
		psad_sub44 = sad_sub44_mmxe;
		psad_00 = sad_00_mmxe;
		psad_01 = sad_01_mmxe;
		psad_10 = sad_10_mmxe;
		psad_11 = sad_11_mmxe;
		pbsad = bsad_mmx;
		pvariance = variance_mmx;
		psumsq = sumsq_mmx;
		pbsumsq = bsumsq_mmx;
		psumsq_sub22 = sumsq_sub22_mmx;
		pbsumsq_sub22 = bsumsq_sub22_mmx;
		pfind_best_one_pel = find_best_one_pel_mmxe;
		pbuild_sub22_mests	= build_sub22_mests_mmxe;
		pbuild_sub44_mests	= build_sub44_mests_mmx;
		pmblocks_sub44_mests = mblocks_sub44_mests_mmxe;
	}
	else if(cpucap & ACCEL_X86_MMX) /* Ordinary MMX CPU */
	{
		mjpeg_info( "SETTING MMX for MOTION!");
		psad_sub22 = sad_sub22_mmx;
		psad_sub44 = sad_sub44_mmx;
		psad_00 = sad_00_mmx;
		psad_01 = sad_01_mmx;
		psad_10 = sad_10_mmx;
		psad_11 = sad_11_mmx;
		pbsad = bsad_mmx;
		pvariance = variance_mmx;
		psumsq = sumsq_mmx;
		pbsumsq = bsumsq_mmx;
		psumsq_sub22 = sumsq_sub22_mmx;
		pbsumsq_sub22 = bsumsq_sub22_mmx;
		// NOT CHANGED pfind_best_one_pel;
		// NOT CHANGED pbuild_sub22_mests	= build_sub22_mests;
		pbuild_sub44_mests	= build_sub44_mests_mmx;
		pmblocks_sub44_mests = mblocks_sub44_mests_mmx;
	}

}
