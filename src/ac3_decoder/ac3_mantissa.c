#include <unistd.h>                                              /* getpid() */

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/soundcard.h>                               /* "audio_output.h" */
#include <sys/uio.h>                                            /* "input.h" */

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "debug.h"                                      /* "input_netlist.h" */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "input.h"                                           /* pes_packet_t */
#include "input_netlist.h"                         /* input_NetlistFreePES() */
#include "decoder_fifo.h"         /* DECODER_FIFO_(ISEMPTY|START|INCSTART)() */

#include "audio_output.h"

#include "ac3_decoder.h"
#include "ac3_mantissa.h"

static float q_1_0[ 32 ] = { (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, 0, 0, 0, 0, 0 };
static float q_1_1[ 32 ] = { (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, 0, 0, 0, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, 0, 0, 0, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, (-2 << 15) / 3, 0, 0, 0, (2 << 15) / 3, (2 << 15) / 3, (2 << 15) / 3, 0, 0, 0, 0, 0 };
static float q_1_2[ 32 ] = { (-2 << 15) / 3, 0, (2 << 15) / 3, (-2 << 15) / 3, 0, (2 << 15) / 3, (-2 << 15) / 3, 0, (2 << 15) / 3, (-2 << 15) / 3, 0, (2 << 15) / 3, (-2 << 15) / 3, 0, (2 << 15) / 3, (-2 << 15) / 3, 0, (2 << 15) / 3, (-2 << 15) / 3, 0, (2 << 15) / 3, (-2 << 15) / 3, 0, (2 << 15) / 3, (-2 << 15) / 3, 0, (2 << 15) / 3, 0, 0, 0, 0, 0 };

static float q_2_0[ 128 ] = { (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, 0, 0, 0 };
static float q_2_1[ 128 ] = { (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, 0, 0, 0, 0, 0, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, 0, 0, 0, 0, 0, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, 0, 0, 0, 0, 0, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, 0, 0, 0, 0, 0, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, (-2 << 15) / 5, 0, 0, 0, 0, 0, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (2 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, (4 << 15) / 5, 0, 0, 0 };
static float q_2_2[ 128 ] = { (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, (-4 << 15) / 5, (-2 << 15) / 5, 0, (2 << 15) / 5, (4 << 15) / 5, 0, 0, 0 };

static float q_4_0[ 128 ] = { (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, (-2 << 15) / 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (2 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, (10 << 15) / 11, 0, 0, 0, 0, 0, 0, 0 };
static float q_4_1[ 128 ] = { (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, (-10 << 15) / 11, (-8 << 15) / 11, (-6 << 15) / 11, (-4 << 15) / 11, (-2 << 15) / 11, 0, (2 << 15) / 11, (4 << 15) / 11, (6 << 15) / 11, (8 << 15) / 11, (10 << 15) / 11, 0, 0, 0, 0, 0, 0, 0 };

//Lookup tables of 0.16 two's complement quantization values

/*
s32 q_1[3] = {( -2 << 15)/3, 0           ,(  2 << 15)/3 };

s32 q_2[5] = {( -4 << 15)/5,( -2 << 15)/5,   0         ,
	                (  2 << 15)/5,(  4 << 15)/5};
*/

static float q_3[7] = {( -6 << 15)/7,( -4 << 15)/7,( -2 << 15)/7,
	                   0         ,(  2 << 15)/7,(  4 << 15)/7,
									(  6 << 15)/7};

/*
s32 q_4[11] = {(-10 << 15)/11,(-8 << 15)/11,(-6 << 15)/11,
	                ( -4 << 15)/11,(-2 << 15)/11,  0          ,
									(  2 << 15)/11,( 4 << 15)/11,( 6 << 15)/11,
									(  8 << 15)/11,(10 << 15)/11};
*/
static float q_5[15] = {(-14 << 15)/15,(-12 << 15)/15,(-10 << 15)/15,
                   ( -8 << 15)/15,( -6 << 15)/15,( -4 << 15)/15,
									 ( -2 << 15)/15,   0          ,(  2 << 15)/15,
									 (  4 << 15)/15,(  6 << 15)/15,(  8 << 15)/15,
									 ( 10 << 15)/15,( 12 << 15)/15,( 14 << 15)/15};

//These store the persistent state of the packed mantissas
static float q_1[2];
static float q_2[2];
static float q_4[1];
static s32 q_1_pointer;
static s32 q_2_pointer;
static s32 q_4_pointer;

//Conversion from bap to number of bits in the mantissas
//zeros account for cases 0,1,2,4 which are special cased
static u16 qnttztab[16] = { 0, 0, 0, 3, 0 , 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16};

static float exp_lut[ 25 ] =
{
	6.10351562500000000000000000e-05,
	3.05175781250000000000000000e-05,
	1.52587890625000000000000000e-05,
	7.62939453125000000000000000e-06,
	3.81469726562500000000000000e-06,
	1.90734863281250000000000000e-06,
	9.53674316406250000000000000e-07,
	4.76837158203125000000000000e-07,
	2.38418579101562500000000000e-07,
	1.19209289550781250000000000e-07,
	5.96046447753906250000000000e-08,
	2.98023223876953125000000000e-08,
	1.49011611938476562500000000e-08,
	7.45058059692382812500000000e-09,
	3.72529029846191406250000000e-09,
	1.86264514923095703125000000e-09,
	9.31322574615478515625000000e-10,
	4.65661287307739257812500000e-10,
	2.32830643653869628906250000e-10,
	1.16415321826934814453125000e-10,
	5.82076609134674072265625000e-11,
	2.91038304567337036132812500e-11,
	1.45519152283668518066406250e-11,
	7.27595761418342590332031250e-12,
	3.63797880709171295166015625e-12,
};

#ifdef DITHER
#if 0
static u32 lfsr_state = 1;

static __inline__ float dither_gen( u16 exp )
{
        int i;
        u32 state;
	s16 mantissa;

        //explicitly bring the state into a local var as gcc > 3.0?
        //doesn't know how to optimize out the stores
        state = lfsr_state;

        //Generate eight pseudo random bits
        for(i=0;i<8;i++)
        {
                state <<= 1;

                if(state & 0x10000)
                        state ^= 0xa011;
        }

        lfsr_state = state;

        mantissa = ((((s32)state<<8)>>8) * (s32) (0.707106f * 256.0f))>>16;
	return( mantissa * exp_lut[exp] );
}
#else
static int fuck[31] =
{
    0x00000001,
    0x00000002,
    0x00000004,
    0x00000008,
    0x00000010,
    0x00000020,
    0x00000040,
    0x00000080,
    0x00000100,
    0x80000200,
    0x00000400,
    0x00000800,
    0x00001000,
    0x00002000,
    0x00004000,
    0x00008000,
    0x80010000,
    0x00020000,
    0x00040000,
    0x00080000,
    0x00100000,
    0x80200000,
    0x00400000,
    0x00800000,
    0x01000000,
    0x02000000,
    0x04000000,
    0x08000000,
    0x10000000,
    0x20000000,
    0x40000000
};

static int index = 0;

static __inline__ float dither_gen( u16 exp )
{
	int tmp;
	tmp = fuck[(index+3)%31];
	tmp ^= fuck[index];
	fuck[index] = tmp;
	index = (index+1)%31;
        return( tmp * 1.52587890625e-5f * 0.707106f * exp_lut[exp] );
}
#endif
#endif

/* Fetch an unpacked, left justified, and properly biased/dithered mantissa value */
#ifdef DITHER
static __inline__ float float_get( ac3dec_thread_t * p_ac3dec, u16 bap, u16 dithflag, u16 exp )
#else
static __inline__ float float_get( ac3dec_thread_t * p_ac3dec, u16 bap, u16 exp )
#endif
{
	u32 group_code;

	//If the bap is 0-5 then we have special cases to take care of
	switch ( bap )
	{
		case 0:
#ifdef DITHER
			if(dithflag)
				return( dither_gen(exp) );
			else
#endif
				return( 0 );

		case 1:
			if ( q_1_pointer >= 0 )
			{
				return( q_1[q_1_pointer--] * exp_lut[exp] );
			}
			NeedBits( &(p_ac3dec->bit_stream), 5 );
			group_code = p_ac3dec->bit_stream.fifo.buffer >> (32 - 5);
			DumpBits( &(p_ac3dec->bit_stream), 5 );
			p_ac3dec->total_bits_read += 5;

			/*
			if(group_code > 26)
				//FIXME do proper block error handling
				printf("\n!! Invalid mantissa !!\n");
			*/

			//q_1[ 0 ] = q_1_0[ group_code ];
			q_1[ 1 ] = q_1_1[ group_code ];
			q_1[ 0 ] = q_1_2[ group_code ];

			q_1_pointer = 1;

			return( q_1_0[group_code] * exp_lut[exp] );

		case 2:
			if ( q_2_pointer >= 0 )
			{
				return( q_2[q_2_pointer--] * exp_lut[exp] );
			}
			NeedBits( &(p_ac3dec->bit_stream), 7 );
			group_code = p_ac3dec->bit_stream.fifo.buffer >> (32 - 7);
			DumpBits( &(p_ac3dec->bit_stream), 7 );
			p_ac3dec->total_bits_read += 7;

			/*
			if(group_code > 124)
				//FIXME do proper block error handling
				printf("\n!! Invalid mantissa !!\n");
			*/

			//q_2[ 0 ] = q_2_0[ group_code ];
			q_2[ 1 ] = q_2_1[ group_code ];
			q_2[ 0 ] = q_2_2[ group_code ];

			q_2_pointer = 1;

			return( q_2_0[ group_code ] * exp_lut[exp] );

		case 3:
			NeedBits( &(p_ac3dec->bit_stream), 3 );
			group_code = p_ac3dec->bit_stream.fifo.buffer >> (32 - 3);
			DumpBits( &(p_ac3dec->bit_stream), 3 );
			p_ac3dec->total_bits_read += 3;

			/*
			if(group_code > 6)
				//FIXME do proper block error handling
				printf("\n!! Invalid mantissa !!\n");
			*/

			return( q_3[group_code] * exp_lut[exp] );

		case 4:
			if ( q_4_pointer >= 0 )
			{
				return( q_4[q_4_pointer--] * exp_lut[exp] );
			}
			NeedBits( &(p_ac3dec->bit_stream), 7 );
			group_code = p_ac3dec->bit_stream.fifo.buffer >> (32 - 7);
			DumpBits( &(p_ac3dec->bit_stream), 7 );
			p_ac3dec->total_bits_read += 7;

			/*
			if(group_code > 120)
				//FIXME do proper block error handling
				printf("\n!! Invalid mantissa !!\n");
			*/

			//q_4[ 0 ] = q_4_0[ group_code ];
			q_4[ 0 ] = q_4_1[ group_code ];

			q_4_pointer = 0;

			return( q_4_0[ group_code ] * exp_lut[exp] );

		case 5:
			NeedBits( &(p_ac3dec->bit_stream), 4 );
			group_code = p_ac3dec->bit_stream.fifo.buffer >> (32 - 4);
			DumpBits( &(p_ac3dec->bit_stream), 4 );
			p_ac3dec->total_bits_read += 4;

			/*
			if(group_code > 14)
				//FIXME do proper block error handling
				printf("\n!! Invalid mantissa !!\n");
			*/

			return( q_5[group_code] * exp_lut[exp] );

		default:
			NeedBits( &(p_ac3dec->bit_stream), qnttztab[bap] );
			group_code = (((s32)(p_ac3dec->bit_stream.fifo.buffer)) >> (32 - qnttztab[bap])) << (16 - qnttztab[bap]);
			DumpBits( &(p_ac3dec->bit_stream), qnttztab[bap] );
			p_ac3dec->total_bits_read += qnttztab[bap];

			return( ((s32)group_code) * exp_lut[exp] );
	}
}

static __inline__ void uncouple_channel( ac3dec_thread_t * p_ac3dec, u32 ch )
{
	u32 bnd = 0;
	u32 i,j;
	float cpl_coord = 0;
	//float coeff;
	u32 cpl_exp_tmp;
	u32 cpl_mant_tmp;

	for(i=p_ac3dec->audblk.cplstrtmant;i<p_ac3dec->audblk.cplendmant;)
	{
		if(!p_ac3dec->audblk.cplbndstrc[bnd])
		{
			cpl_exp_tmp = p_ac3dec->audblk.cplcoexp[ch][bnd] + 3 * p_ac3dec->audblk.mstrcplco[ch];
			if(p_ac3dec->audblk.cplcoexp[ch][bnd] == 15)
				cpl_mant_tmp = (p_ac3dec->audblk.cplcomant[ch][bnd]) << 12;
			else
				cpl_mant_tmp = ((0x10) | p_ac3dec->audblk.cplcomant[ch][bnd]) << 11;

			cpl_coord = ((s16)cpl_mant_tmp) * exp_lut[cpl_exp_tmp];
		}
		bnd++;

		for(j=0;j < 12; j++)
		{
			//Get new dither values for each channel if necessary, so
			//the channels are uncorrelated
#ifdef DITHER
			if ( p_ac3dec->audblk.dithflag[ch] && p_ac3dec->audblk.cpl_bap[i] == 0 )
				p_ac3dec->coeffs.fbw[ch][i] = cpl_coord * dither_gen( p_ac3dec->audblk.cpl_exp[i] );
			else
#endif
				p_ac3dec->coeffs.fbw[ch][i]  = cpl_coord * p_ac3dec->audblk.cplfbw[i];
			i++;
		}
	}
}

#if 0
void
uncouple(bsi_t *bsi,audblk_t *audblk,stream_coeffs_t *coeffs)
{
	int i,j;

	for(i=0; i< bsi->nfchans; i++)
	{
		for(j=0; j < audblk->endmant[i]; j++)
			 convert_to_float(audblk->fbw_exp[i][j],audblk->chmant[i][j],
					 (u32*) &coeffs->fbw[i][j]);
	}

	if(audblk->cplinu)
	{
		for(i=0; i< bsi->nfchans; i++)
		{
			if(audblk->chincpl[i])
			{
				uncouple_channel(coeffs,audblk,i);
			}
		}

	}

	if(bsi->lfeon)
	{
		/* There are always 7 mantissas for lfe */
		for(j=0; j < 7 ; j++)
			 convert_to_float(audblk->lfe_exp[j],audblk->lfemant[j],
					 (u32*) &coeffs->lfe[j]);

	}

}
#endif

/*
void mantissa_unpack( bsi_t * bsi, audblk_t * audblk, bitstream_t * bs )
*/
void mantissa_unpack( ac3dec_thread_t * p_ac3dec )
{
	int i, j;

	q_1_pointer = -1;
	q_2_pointer = -1;
	q_4_pointer = -1;

	if ( p_ac3dec->audblk.cplinu )
	{
		/* 1 */
		for ( i = 0; !p_ac3dec->audblk.chincpl[i]; i++ )
		{
			for ( j = 0; j < p_ac3dec->audblk.endmant[i]; j++ )
			{
#ifdef DITHER
				p_ac3dec->coeffs.fbw[i][j] = float_get( p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.dithflag[i], p_ac3dec->audblk.fbw_exp[i][j] );
#else
				p_ac3dec->coeffs.fbw[i][j] = float_get( p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.fbw_exp[i][j] );
#endif
			}
		}

		/* 2 */
		for ( j = 0; j < p_ac3dec->audblk.endmant[i]; j++ )
		{
#ifdef DITHER
			p_ac3dec->coeffs.fbw[i][j] = float_get( p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.dithflag[i], p_ac3dec->audblk.fbw_exp[i][j] );
#else
			p_ac3dec->coeffs.fbw[i][j] = float_get( p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.fbw_exp[i][j] );
#endif
		}
		for ( j = p_ac3dec->audblk.cplstrtmant; j < p_ac3dec->audblk.cplendmant; j++ )
		{
#ifdef DITHER
			p_ac3dec->audblk.cplfbw[j] = float_get( p_ac3dec, p_ac3dec->audblk.cpl_bap[j], 0, p_ac3dec->audblk.cpl_exp[j] );
#else
			p_ac3dec->audblk.cplfbw[j] = float_get( p_ac3dec, p_ac3dec->audblk.cpl_bap[j], p_ac3dec->audblk.cpl_exp[j] );
#endif
		}
//		uncouple_channel( coeffs, audblk, i );

		/* 3 */
		for ( i++; i < p_ac3dec->bsi.nfchans; i++ )
		{
			for ( j = 0; j < p_ac3dec->audblk.endmant[i]; j++ )
			{
#ifdef DITHER
				p_ac3dec->coeffs.fbw[i][j] = float_get( p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.dithflag[i], p_ac3dec->audblk.fbw_exp[i][j] );
#else
				p_ac3dec->coeffs.fbw[i][j] = float_get( p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.fbw_exp[i][j] );
#endif
			}
			if ( p_ac3dec->audblk.chincpl[i] )
			{
//				uncouple_channel( coeffs, audblk, i );
			}
		}

		for ( i = 0; i < p_ac3dec->bsi.nfchans; i++ )
		{
			if ( p_ac3dec->audblk.chincpl[i] )
			{
				uncouple_channel( p_ac3dec, i );
			}
		}
	}
	else
	{
		for ( i = 0; i < p_ac3dec->bsi.nfchans; i++ )
		{
			for ( j = 0; j < p_ac3dec->audblk.endmant[i]; j++ )
			{
#ifdef DITHER
				p_ac3dec->coeffs.fbw[i][j] = float_get( p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.dithflag[i], p_ac3dec->audblk.fbw_exp[i][j] );
#else
				p_ac3dec->coeffs.fbw[i][j] = float_get( p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.fbw_exp[i][j] );
#endif
			}
		}
	}

	if ( p_ac3dec->bsi.lfeon )
	{
		/* There are always 7 mantissas for lfe, no dither for lfe */
		for ( j = 0; j < 7; j++ )
		{
#ifdef DITHER
			p_ac3dec->coeffs.lfe[j] = float_get( p_ac3dec, p_ac3dec->audblk.lfe_bap[j], 0, p_ac3dec->audblk.lfe_exp[j] );
#else
			p_ac3dec->coeffs.lfe[j] = float_get( p_ac3dec, p_ac3dec->audblk.lfe_bap[j], p_ac3dec->audblk.lfe_exp[j] );
#endif
		}
	}
}
