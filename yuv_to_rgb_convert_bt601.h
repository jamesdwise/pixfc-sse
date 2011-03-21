/*
 * yuv_to_rgb_convert_bt601.h
 *
 * Copyright (C) 2011 PixFC Team (pixelfc@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public  License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef YUV_TO_RGB_CONVERT_BT_601_H_
#define YUV_TO_RGB_CONVERT_BT_601_H_


#include <emmintrin.h>
#include <tmmintrin.h>

#include "common.h"
#include "platform_util.h"


/*
 * Convert 2 vectors of 8 short Y, UY into 3 vectors of 8 short R, G & B
 * using BT601 YCbCr to RGB conversion equations from
 * http://www.equasys.de/colorconversion.html
 *
 *
 * Total latency: 			68 cycles
 * Num of pixel handled:	8
 *
 * R = 	[ 1.164		0		1.596	]	( Y - 16)
 * G = 	[ 1.164		-0.392	-0.813	]	( U - 128 )
 * B = 	[ 1.164		3.017	0		]	( V - 128 )
 *
 * Y coeffs left shifted by 15 bits
 * U & V coeffs left shifted by 7 bits
 * 		[ 38142		0		204		]
 * 		[ 38142		-50		-104	]
 * 		[ 38142		386		0		]
 *
 *
 * INPUT:
 *
 * 2 vectors of 8 short:
 * yVect
 * Y1 0		Y2 0	Y3 0	Y4 0	Y5 0	Y6 0	Y7 0	Y8 0
 *
 * uvVect
 * U12 0	V12 0	U34 0	V34 0	U56 0	V56 0	U78 0	V78 0
 *
 * OUTPUT:
 *
 * 3 vectors of 8 short:
 * rVect
 * R1 0		R2 0	R3 0	R4 0	R5 0	R6 0	R7 0	R8 0
 *
 * gVect
 * G1 0		G2 0	G3 0	G4 0	G5 0	G6 0	G7 0	G8 0
 *
 * bVect
 * B1 0		B2 0	B3 0	B4 0	B5 0	B6 0	B7 0	B8 0
 */
EXTERN_INLINE void convert_y_uv_vectors_to_rgb_vectors_bt601_no_interpolation_sse2(__m128i* in_2_v16i_y_uv_vectors, __m128i* out_3_v16i_rgb_vectors) 
{
	CONST_M128I(sub128, 0xFF80FF80FF80FF80LL, 0xFF80FF80FF80FF80LL);
	CONST_M128I(sub16, 0xFFF0FFF0FFF0FFF0LL, 0xFFF0FFF0FFF0FFF0LL);
	CONST_M128I(yCoeffs, 0x94FE94FE94FE94FELL, 0x94FE94FE94FE94FELL);
	M128I(uvRCoeffs, 0x00CC000000CC0000LL, 0x00CC000000CC0000LL);
	M128I(uvGCoeffs, 0xFF98FFCEFF98FFCELL, 0xFF98FFCEFF98FFCELL);
	M128I(uvBCoeffs, 0x0000018200000182LL, 0x0000018200000182LL);

	// U - 128	V - 128
	in_2_v16i_y_uv_vectors[1] = _mm_add_epi16(in_2_v16i_y_uv_vectors[1], _M(sub128));// PADDW		2	2

	// Y - 16
	in_2_v16i_y_uv_vectors[0] = _mm_add_epi16(in_2_v16i_y_uv_vectors[0], _M(sub16));// PADDW		2	2

	//
	// Y values
	//
	// left shift Y values by 8 bits
	// Ymin << 8 = 0 		Ymax << 8 = 235 << 8 = 60160
	in_2_v16i_y_uv_vectors[0] = _mm_slli_epi16(in_2_v16i_y_uv_vectors[0], 8);		// PSLLW		2	2

	// Multiply 8-bit left-shifted Y values by 15-bit left-shifted Y coeffs 
	// and keep highest 16 bits
	in_2_v16i_y_uv_vectors[0] = _mm_mulhi_epu16(in_2_v16i_y_uv_vectors[0], _M(yCoeffs));// PMULHUW	9 8 2 2

	//
	// We have multiplied 8-bit left-shifted Y values by 15-bit left shifted Y coeffs
	// Overall, we have left-shifted the result by 8 + 15 = 23 bits, which we must undo.
	// PMULHUW has inherently done a 16-bit right shift, so we still have to right
	// shift by 7 bits.
	in_2_v16i_y_uv_vectors[0] = _mm_srli_epi16(in_2_v16i_y_uv_vectors[0], 7);		// PSRLD		2	2
	// Y1 	Y2		Y3		Y4		Y5		Y6		Y7		Y8

	//
	// R
	// U and V coefficients
	// 0, 204, 0, 204, 0, 204, 0, 204
	_M(uvRCoeffs) = _mm_madd_epi16(in_2_v16i_y_uv_vectors[1], _M(uvRCoeffs));		// PMADDWD		9 8 2 2
	// U12*0 + V12*204	U34*0 + V34*204	U56*0 + V56*204	U78*0 + V78*204
	// C12				C34				C56				C78		(4 * 32-bits values)
	// A B Sb Sb 		upper 16 bits are always the sign bit due to the coeffs and pixels values
	//					max value with 7bit left shift of coeffs:
	//					386*127 = 49022 (16 bit wide)
	//					min value:
	//					386*-128= 2's(49408) (49408 fits in 16 bit)
	
	// shift right by 7 to account for left shift by 7 of coefficients
	_M(uvRCoeffs) = _mm_srai_epi32(_M(uvRCoeffs), 7);								// PSRAD		2	2
	// C12 0 0 0		C34 0 0 0		C56 0 0 0		C78 0 0 0
	
	_M(uvRCoeffs) = _mm_shufflehi_epi16(_M(uvRCoeffs), 0xA0);						// PSHUFHW		2	2
	// C12 0 0 0		C34 0 0 0		C56 0 C56 0		C78 0 C78 0
	
	_M(uvRCoeffs) = _mm_shufflelo_epi16(_M(uvRCoeffs), 0xA0);						// PSHUFLW		2	2
	// C12 0 C12 0		C34 0 C34 0		C56 0 C56 0		C78 0 C78 0
	
	out_3_v16i_rgb_vectors[0] = _mm_add_epi16(in_2_v16i_y_uv_vectors[0], _M(uvRCoeffs)); // PADDW	2	2
	//
	
	
	//
	// G
	// U and V coeffs ()
	// -50, -104, -50, -104, -50, -104, -50, -104
	_M(uvGCoeffs) = _mm_madd_epi16(in_2_v16i_y_uv_vectors[1], _M(uvGCoeffs));		// PMADDWD		9 8 2 2
	// U12*-50 + V12*-104	U34*-50 + V34*-104	U56*-50 + V56*-104	U78*-50 + V78*-104
	// C12		C34		C56		C78
	
	// shift right by 7
	_M(uvGCoeffs) = _mm_srai_epi32(_M(uvGCoeffs), 7);								// PSRAD		2	2
	// C12 0 0 0		C34 0 0 0		C56 0 0 0		C78 0 0 0
	
	_M(uvGCoeffs) = _mm_shufflehi_epi16(_M(uvGCoeffs), 0xA0);						// PSHUFHW		2	2
	// C12 0 0 0		C34 0 0 0		C56 0 C56 0		C78 0 C78 0
	
	_M(uvGCoeffs) = _mm_shufflelo_epi16(_M(uvGCoeffs), 0xA0);						// PSHUFLW		2	2
	// C12 0 C12 0		C34 0 C34 0		C56 0 C56 0		C78 0 C78 0
	
	out_3_v16i_rgb_vectors[1] = _mm_add_epi16(in_2_v16i_y_uv_vectors[0], _M(uvGCoeffs));// PADDW	2	2
	//
	
	//
	// B
	// U and V coeffs ()
	// 0, 386, 0, 386, 0, 386, 0, 386
	_M(uvBCoeffs) = _mm_madd_epi16(in_2_v16i_y_uv_vectors[1], _M(uvBCoeffs));		// PMADDWD		9 8 2 2
	// U12*0 + V12*386	U34*0 + V34*386	U56*0 + V56*386	U78*0 + V78*386
	// 0 C12		0 C34		0 C56		0 C78
	
	// shift right by 7
	_M(uvBCoeffs) = _mm_srai_epi32(_M(uvBCoeffs), 7);								// PSRAD		2	2
	// C12	0		C34	 0		C56	 0		C78	 0
	
	_M(uvBCoeffs) = _mm_shufflehi_epi16(_M(uvBCoeffs), 0xA0);						// PSHUFHW		2	2
	// C12 0 0 0		C34 0 0 0		C56 0 C56 0		C78 0 C78 0
	
	_M(uvBCoeffs) = _mm_shufflelo_epi16(_M(uvBCoeffs), 0xA0);						// PSHUFLW		2	2
	// C12 0 C12 0		C34 0 C12 0		C56 0 C56 0		C78 0 C78 0
	
	out_3_v16i_rgb_vectors[2] = _mm_add_epi16(in_2_v16i_y_uv_vectors[0],  _M(uvBCoeffs));// PADDW	2	2
	//
};


/*
 * Convert 3 vectors of 8 short Y, UVOdd, UVEven into 3 vectors of 8 short R, G & B
 * using BT601 YCbCr to RGB conversion equations from
 * http://www.equasys.de/colorconversion.html
 *
 *
 * Total latency: 			109 cycles
 * Num of pixel handled:	8
 *
 * R = 	[ 1.164		0		1.596		]	( Y - 16)
 * G = 	[ 1.164		-0.392	-0.813	]	( U - 128 )
 * B = 	[ 1.164		3.017	0		]	( V - 128 )
 *
 * Y coeffs left shifted by 15 bits
 * U & V coeffs left shifted by 7 bits
 * 		[ 38142		0		204		]	
 * 		[ 38142		-50		-104	]
 * 		[ 38142		386		0		]
 * 
 *
 * INPUT:
 *
 * 3 vectors of 8 short:
 * yVect
 * Y1 0		Y2 0	Y3 0	Y4 0	Y5 0	Y6 0	Y7 0	Y8 0
 *
 * uvVect Odd
 * U1 0		V1 0	U3 0	V3 0	U5 0	V5 0	U7 0	V7 0
 *
 * uvVect Even
 * U2 0		V2 0	U4 0	V4 0	U6 0	V6 0	U8 0	V8 0
 *
 * OUTPUT:
 *
 * 3 vectors of 8 short:
 * rVect
 * R1 0		R2 0	R3 0	R4 0	R5 0	R6 0	R7 0	R8 0
 *
 * gVect
 * G1 0		G2 0	G3 0	G4 0	G5 0	G6 0	G7 0	G8 0
 *
 * bVect
 * B1 0		B2 0	B3 0	B4 0	B5 0	B6 0	B7 0	B8 0
 */
EXTERN_INLINE void convert_y_uv_vectors_to_rgb_vectors_bt601_sse2(__m128i* in_3_v16i_y_uvOdd_uvEven_vectors, __m128i* out_3_v16i_rgb_vectors)
{
	CONST_M128I(sub128, 0xFF80FF80FF80FF80LL, 0xFF80FF80FF80FF80LL);
	CONST_M128I(sub16, 0xFFF0FFF0FFF0FFF0LL, 0xFFF0FFF0FFF0FFF0LL);
	CONST_M128I(yCoeffs, 0x94FE94FE94FE94FELL, 0x94FE94FE94FE94FELL);
	CONST_M128I(uvRCoeffs, 0x00CC000000CC0000LL, 0x00CC000000CC0000LL);
	CONST_M128I(uvGCoeffs, 0xFF98FFCEFF98FFCELL, 0xFF98FFCEFF98FFCELL);
	CONST_M128I(uvBCoeffs, 0x0000018200000182LL, 0x0000018200000182LL);
	CONST_M128I(zeroHiWord, 0x0000FFFF0000FFFFLL, 0x0000FFFF0000FFFFLL);
	CONST_M128I(zeroLoWord, 0xFFFF0000FFFF0000LL, 0xFFFF0000FFFF0000LL);
	M128I(uvOdd, 0x0LL, 0x0LL);
	M128I(uvEven, 0x0LL, 0x0LL);

	// Y - 16
	in_3_v16i_y_uvOdd_uvEven_vectors[0] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(sub16));	// PADDW		2	2

	// U - 128	V - 128
	in_3_v16i_y_uvOdd_uvEven_vectors[1] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[1], _M(sub128));	// PADDW		2	2
	in_3_v16i_y_uvOdd_uvEven_vectors[2] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[2], _M(sub128));	// PADDW		2	2

	//
	// Y values
	//
	// left shift Y values by 8 bits
	// Ymin << 8 = 0 		Ymax << 8 = 235 << 8 = 60160
	in_3_v16i_y_uvOdd_uvEven_vectors[0] = _mm_slli_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], 8);			// PSLLW		2	2

	// Multiply 8-bit left-shifted Y values by 15-bit left-shifted Y coeffs 
	// and keep highest 16 bits
	in_3_v16i_y_uvOdd_uvEven_vectors[0] = _mm_mulhi_epu16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(yCoeffs));// PMULHUW		9 8 2 2

	//
	// We have multiplied 8-bit left-shifted Y values by 15-bit left shifted Y coeffs
	// Overall, we have left-shifted the result by 8 + 15 = 23 bits, which we must undo.
	// PMULHUW has inherently done a 16-bit right shift, so we still have to right
	// shift by 7 bits.
	in_3_v16i_y_uvOdd_uvEven_vectors[0] = _mm_srli_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], 7);			// PSRLD		2	2
	// Y1 	Y2		Y3		Y4		Y5		Y6		Y7		Y8

	//
	// R
	// U and V coefficients
	// 0, 204, 0, 204, 0, 204, 0, 204
	_M(uvOdd) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[1], _M(uvRCoeffs));	// PMADDWD		9 8 2 2
	// U1*0 + V1*204	U3*0 + V3*204	U5*0 + V5*204	U7*0 + V7*204
	// C1			C3				C5				C7		(4 * 32-bits values)
	// A B Sb Sb 		upper 16 bits are always the sign bit due to the coeffs and pixels values
	//					max value with 7bit left shift of coeffs:
	//					386*127 = 49022 (16 bit wide)
	//					min value:
	//					386*-128= 2's(49408) (49408 fits in 16 bit)
	
	// shift right by 7 to account for left shift by 7 of coefficients
	_M(uvOdd) = _mm_and_si128(_mm_srai_epi32(_M(uvOdd), 7), _M(zeroHiWord));		// PSRAD		2	2
	// C1 0 0 0		C3 0 0 0		C5 0 0 0		C7 0 0 0						// PAND			2	2

	_M(uvEven) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[2], _M(uvRCoeffs));// PMADDWD		9 8 2 2
	// U2*0 + V2*204	U4*0 + V4*204	U6*0 + V6*204	U8*0 + V8*204
	// C2			C4				C6				C8
	
	// Move these values to the hi word and right shift by 7 to account for 7-bit left shift
	// of coefficients: 16 - 7 = 9 - we need to left shift by 9 bits.
	_M(uvEven) = _mm_and_si128(_mm_slli_epi32(_M(uvEven), 9), _M(zeroLoWord));		// PSLLD		2	2
	// 0 C2			0 C4		0 C6		0 C8									// PAND			2	2
	
	_M(uvEven) = _mm_or_si128(_M(uvEven), _M(uvOdd));								// POR			2	2
	// C1 C2		C3 C4		C5 C6		C7 C8
	
	out_3_v16i_rgb_vectors[0] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(uvEven));	// PADDW	2	2


	
	//
	// G
	// U and V coeffs
	// -50, -104, -50, -104, -50, -104, -50, -104,
	_M(uvOdd) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[1], _M(uvGCoeffs));	// PMADDWD		9 8 2 2
	// C1			C3				C5				C7		(4 * 32-bits values)
	
	_M(uvOdd) = _mm_and_si128(_mm_srai_epi32(_M(uvOdd), 7), _M(zeroHiWord));		// PSRAD		2	2
	// C1 0 0 0		C3 0 0 0		C5 0 0 0		C7 0 0 0						// PAND			2	2

	_M(uvEven) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[2], _M(uvGCoeffs));// PMADDWD		9 8 2 2
	// U2*0 + V2*204	U4*0 + V4*204	U6*0 + V6*204	U8*0 + V8*204
	// C2			C4				C6				C8
	
	_M(uvEven) = _mm_and_si128(_mm_slli_epi32(_M(uvEven), 9), _M(zeroLoWord));		// PSLLD		2	2
	// 0 C2			0 C4		0 C6		0 C8									// PAND			2	2
	
	_M(uvEven) = _mm_or_si128(_M(uvEven), _M(uvOdd));								// POR			2	2
	// C1 C2		C3 C4		C5 C6		C7 C8
	
	out_3_v16i_rgb_vectors[1] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(uvEven));	// PADDW	2	2
	
	
	
	//
	// B
	// U and V coeffs
	// 386, 0, 386, 0, 386, 0, 386, 0,
	_M(uvOdd) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[1], _M(uvBCoeffs));	// PMADDWD		9 8 2 2
	// C1			C3				C5				C7		(4 * 32-bits values)

	_M(uvOdd) = _mm_and_si128(_mm_srai_epi32(_M(uvOdd), 7), _M(zeroHiWord));		// PSRAD		2	2
	// C1 0 0 0		C3 0 0 0		C5 0 0 0		C7 0 0 0						// PAND			2	2

	_M(uvEven) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[2], _M(uvBCoeffs));// PMADDWD		9 8 2 2
	// U2*0 + V2*204	U4*0 + V4*204	U6*0 + V6*204	U8*0 + V8*204
	// C2			C4				C6				C8

	_M(uvEven) = _mm_and_si128(_mm_slli_epi32(_M(uvEven), 9), _M(zeroLoWord));		// PSLLD		2	2
	// 0 C2			0 C4		0 C6		0 C8									// PAND			2	2

	_M(uvEven) = _mm_or_si128(_M(uvEven), _M(uvOdd));								// POR			2	2
	// C1 C2		C3 C4		C5 C6		C7 C8

	out_3_v16i_rgb_vectors[2] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(uvEven));	// PADDW	2	2
};




/*
 * Convert 2 vectors of 8 short Y, UV into 3 vectors of 8 short R, G & B
 * using BT601 YCbCr to RGB conversion equations from
 * http://www.equasys.de/colorconversion.html
 *
 *
 * Total latency: 			59 cycles
 * Num of pixel handled:	8
 *
 * R = 	[ 1.164		0		1.596	]	( Y - 16)
 * G = 	[ 1.164		-0.392	-0.813	]	( U - 128 )
 * B = 	[ 1.164		3.017	0		]	( V - 128 )
 *
 * Y coeffs left shifted by 15 bits
 * U & V coeffs left shifted by 7 bits
 * 		[ 38142		0		204		]
 * 		[ 38142		-50		-104	]
 * 		[ 38142		386		0		]
 *
 *
 * INPUT:
 *
 * 2 vectors of 8 short:
 * yVect
 * Y1 0		Y2 0	Y3 0	Y4 0	Y5 0	Y6 0	Y7 0	Y8 0
 *
 * uvVect
 * U12 0	V12 0	U34 0	V34 0	U56 0	V56 0	U78 0	V78 0
 *
 * OUTPUT:
 *
 * 3 vectors of 8 short:
 * rVect
 * R1 0		R2 0	R3 0	R4 0	R5 0	R6 0	R7 0	R8 0
 *
 * gVect
 * G1 0		G2 0	G3 0	G4 0	G5 0	G6 0	G7 0	G8 0
 *
 * bVect
 * B1 0		B2 0	B3 0	B4 0	B5 0	B6 0	B7 0	B8 0
 */
EXTERN_INLINE void convert_y_uv_vectors_to_rgb_vectors_bt601_no_interpolation_sse2_ssse3(__m128i* in_2_v16i_y_uv_vectors, __m128i* out_3_v16i_rgb_vectors)
{
	CONST_M128I(shuffMask, 0x0504050401000100LL, 0x0D0C0D0C09080908LL);
	CONST_M128I(sub128, 0xFF80FF80FF80FF80LL, 0xFF80FF80FF80FF80LL);
	CONST_M128I(sub16, 0xFFF0FFF0FFF0FFF0LL, 0xFFF0FFF0FFF0FFF0LL);
	CONST_M128I(yCoeffs, 0x94FE94FE94FE94FELL, 0x94FE94FE94FE94FELL);
	M128I(uvRCoeffs, 0x00CC000000CC0000LL, 0x00CC000000CC0000LL);
	M128I(uvGCoeffs, 0xFF98FFCEFF98FFCELL, 0xFF98FFCEFF98FFCELL);
	M128I(uvBCoeffs, 0x0000018200000182LL, 0x0000018200000182LL);

	// U - 128	V - 128
	in_2_v16i_y_uv_vectors[1] = _mm_add_epi16(in_2_v16i_y_uv_vectors[1], _M(sub128));// PADDW		2	2

	// Y - 16
	in_2_v16i_y_uv_vectors[0] = _mm_add_epi16(in_2_v16i_y_uv_vectors[0], _M(sub16));// PADDW		2	2

	//
	// Y values
	//
	// left shift Y values by 8 bits
	// Ymin << 8 = 0 		Ymax << 8 = 235 << 8 = 60160
	in_2_v16i_y_uv_vectors[0] = _mm_slli_epi16(in_2_v16i_y_uv_vectors[0], 8);		// PSLLW		2	2

	// Multiply 8-bit left-shifted Y values by 15-bit left-shifted Y coeffs
	// and keep highest 16 bits
	in_2_v16i_y_uv_vectors[0] = _mm_mulhi_epu16(in_2_v16i_y_uv_vectors[0], _M(yCoeffs));// PMULHUW	9 8 2 2

	//
	// We have multiplied 8-bit left-shifted Y values by 15-bit left shifted Y coeffs
	// Overall, we have left-shifted the result by 8 + 15 = 23 bits, which we must undo.
	// PMULHUW has inherently done a 16-bit right shift, so we still have to right
	// shift by 7 bits.
	in_2_v16i_y_uv_vectors[0] = _mm_srli_epi16(in_2_v16i_y_uv_vectors[0], 7);		// PSRLD		2	2
	// Y1 	Y2		Y3		Y4		Y5		Y6		Y7		Y8

	//
	// R
	// U and V coefficients
	// 0, 204, 0, 204, 0, 204, 0, 204
	_M(uvRCoeffs) = _mm_madd_epi16(in_2_v16i_y_uv_vectors[1], _M(uvRCoeffs));		// PMADDWD		9 8 2 2
	// U12*0 + V12*204	U34*0 + V34*204	U56*0 + V56*204	U78*0 + V78*204
	// C12				C34				C56				C78		(4 * 32-bits values)
	// A B Sb Sb 		upper 16 bits are always the sign bit due to the coeffs and pixels values
	//					max value with 7bit left shift of coeffs:
	//					386*127 = 49022 (16 bit wide)
	//					min value:
	//					386*-128= 2's(49408) (49408 fits in 16 bit)

	// shift right by 7 to account for left shift by 7 of coefficients
	_M(uvRCoeffs) = _mm_srai_epi32(_M(uvRCoeffs), 7);								// PSRAD		2	2
	// C12 0 0 0		C34 0 0 0		C56 0 0 0		C78 0 0 0

	_M(uvRCoeffs) = _mm_shuffle_epi8 (_M(uvRCoeffs), _M(shuffMask));				// PSHUFB		1 1 3   0.5 1 2
	// C12 C12		C34 C34 	C56 C56		C78 C78

	out_3_v16i_rgb_vectors[0] = _mm_add_epi16(in_2_v16i_y_uv_vectors[0], _M(uvRCoeffs)); // PADDW	2	2
	//


	//
	// G
	// U and V coeffs ()
	// -50, -104, -50, -104, -50, -104, -50, -104
	_M(uvGCoeffs) = _mm_madd_epi16(in_2_v16i_y_uv_vectors[1], _M(uvGCoeffs));		// PMADDWD		9 8 2 2
	// U12*-50 + V12*-104	U34*-50 + V34*-104	U56*-50 + V56*-104	U78*-50 + V78*-104
	// C12		C34		C56		C78

	// shift right by 7
	_M(uvGCoeffs) = _mm_srai_epi32(_M(uvGCoeffs), 7);								// PSRAD		2	2
	// C12 0 0 0		C34 0 0 0		C56 0 0 0		C78 0 0 0

	_M(uvGCoeffs) = _mm_shuffle_epi8 (_M(uvGCoeffs), _M(shuffMask));				// PSHUFB		1 1 3   0.5 1 2
	// C12 C12		C34 C34 	C56 C56		C78 C78

	out_3_v16i_rgb_vectors[1] = _mm_add_epi16(in_2_v16i_y_uv_vectors[0], _M(uvGCoeffs));// PADDW	2	2
	//

	//
	// B
	// U and V coeffs ()
	// 0, 386, 0, 386, 0, 386, 0, 386
	_M(uvBCoeffs) = _mm_madd_epi16(in_2_v16i_y_uv_vectors[1], _M(uvBCoeffs));		// PMADDWD		9 8 2 2
	// U12*0 + V12*386	U34*0 + V34*386	U56*0 + V56*386	U78*0 + V78*386
	// 0 C12		0 C34		0 C56		0 C78

	// shift right by 7
	_M(uvBCoeffs) = _mm_srai_epi32(_M(uvBCoeffs), 7);								// PSRAD		2	2
	// C12	0		C34	 0		C56	 0		C78	 0

	_M(uvBCoeffs) = _mm_shuffle_epi8 (_M(uvBCoeffs), _M(shuffMask));				// PSHUFB		1 1 3   0.5 1 2
	// C12 C12		C34 C34 	C56 C56		C78 C78

	out_3_v16i_rgb_vectors[2] = _mm_add_epi16(in_2_v16i_y_uv_vectors[0],  _M(uvBCoeffs));// PADDW	2	2
	//
};




/*
 * Convert 3 vectors of 8 short Y, UVOdd, UVEven into 3 vectors of 8 short R, G & B
 * using BT601 YCbCr to RGB conversion equations from
 * http://www.equasys.de/colorconversion.html
 *
 *
 * Total latency: 			103 cycles
 * Num of pixel handled:	8
 *
 * R = 	[ 1.164		0		1.596	]	( Y - 16)
 * G = 	[ 1.164		-0.392	-0.813	]	( U - 128 )
 * B = 	[ 1.164		3.017	0		]	( V - 128 )
 *
 * Y coeffs left shifted by 15 bits
 * U & V coeffs left shifted by 7 bits
 * 		[ 38142		0		204		]
 * 		[ 38142		-50		-104	]
 * 		[ 38142		386		0		]
 *
 *
 * INPUT:
 *
 * 3 vectors of 8 short:
 * yVect
 * Y1 0		Y2 0	Y3 0	Y4 0	Y5 0	Y6 0	Y7 0	Y8 0
 *
 * uvVect Odd
 * U1 0		V1 0	U3 0	V3 0	U5 0	V5 0	U7 0	V7 0
 *
 * uvVect Even
 * U2 0		V2 0	U4 0	V4 0	U6 0	V6 0	U8 0	V8 0
 *
 * OUTPUT:
 *
 * 3 vectors of 8 short:
 * rVect
 * R1 0		R2 0	R3 0	R4 0	R5 0	R6 0	R7 0	R8 0
 *
 * gVect
 * G1 0		G2 0	G3 0	G4 0	G5 0	G6 0	G7 0	G8 0
 *
 * bVect
 * B1 0		B2 0	B3 0	B4 0	B5 0	B6 0	B7 0	B8 0
 */
EXTERN_INLINE void convert_y_uv_vectors_to_rgb_vectors_bt601_sse2_ssse3(__m128i* in_3_v16i_y_uvOdd_uvEven_vectors, __m128i* out_3_v16i_rgb_vectors)
{
	CONST_M128I(sub128, 0xFF80FF80FF80FF80LL, 0xFF80FF80FF80FF80LL);
	CONST_M128I(sub16, 0xFFF0FFF0FFF0FFF0LL, 0xFFF0FFF0FFF0FFF0LL);
	CONST_M128I(yCoeffs, 0x94FE94FE94FE94FELL, 0x94FE94FE94FE94FELL);
	CONST_M128I(uvRCoeffs, 0x00CC000000CC0000LL, 0x00CC000000CC0000LL);
	CONST_M128I(uvGCoeffs, 0xFF98FFCEFF98FFCELL, 0xFF98FFCEFF98FFCELL);
	CONST_M128I(uvBCoeffs, 0x0000018200000182LL, 0x0000018200000182LL);
	CONST_M128I(shuff1,	0xFFFF0504FFFF0100LL, 0xFFFF0D0CFFFF0908LL);
	CONST_M128I(shuff2,	0x0504FFFF0100FFFFLL, 0x0D0CFFFF0908FFFFLL);
	M128I(uvOdd, 0x0LL, 0x0LL);
	M128I(uvEven, 0x0LL, 0x0LL);

	// Y - 16
	in_3_v16i_y_uvOdd_uvEven_vectors[0] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(sub16));	// PADDW		2	2

	// U - 128	V - 128
	in_3_v16i_y_uvOdd_uvEven_vectors[1] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[1], _M(sub128));	// PADDW		2	2
	in_3_v16i_y_uvOdd_uvEven_vectors[2] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[2], _M(sub128));	// PADDW		2	2

	//
	// Y values
	//
	// left shift Y values by 8 bits
	// Ymin << 8 = 0 		Ymax << 8 = 235 << 8 = 60160
	in_3_v16i_y_uvOdd_uvEven_vectors[0] = _mm_slli_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], 8);			// PSLLW		2	2

	// Multiply 8-bit left-shifted Y values by 15-bit left-shifted Y coeffs
	// and keep highest 16 bits
	in_3_v16i_y_uvOdd_uvEven_vectors[0] = _mm_mulhi_epu16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(yCoeffs));// PMULHUW		9 8 2 2

	//
	// We have multiplied 8-bit left-shifted Y values by 15-bit left shifted Y coeffs
	// Overall, we have left-shifted the result by 8 + 15 = 23 bits, which we must undo.
	// PMULHUW has inherently done a 16-bit right shift, so we still have to right
	// shift by 7 bits.
	in_3_v16i_y_uvOdd_uvEven_vectors[0] = _mm_srli_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], 7);			// PSRLD		2	2
	// Y1 	Y2		Y3		Y4		Y5		Y6		Y7		Y8

	//
	// R
	// U and V coefficients
	// 0, 204, 0, 204, 0, 204, 0, 204
	_M(uvOdd) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[1], _M(uvRCoeffs));	// PMADDWD		9 8 2 2
	// U1*0 + V1*204	U3*0 + V3*204	U5*0 + V5*204	U7*0 + V7*204
	// C1			C3			C5			C7		(4 * 32-bit values)
	// A B Sb Sb 		upper 16 bits are always the sign bit due to the coeffs and pixels values
	//					max value with 7bit left shift of coeffs:
	//					386*127 = 49022 (16 bit wide)
	//					min value:
	//					386*-128= 2's(49408) (49408 fits in 16 bit)

	// shift right by 7 to account for left shift by 7 of coefficients
	_M(uvOdd) = _mm_srai_epi32(_M(uvOdd), 7);										// PSRAD		2	2
	// C1 Sb Sb Sb	C3 Sb Sb Sb	C5 Sb Sb Sb	C7 Sb Sb Sb (4 * 32-bit values)

	// shuffle bytes: keep lo word and zero hi word
	_M(uvOdd) = _mm_shuffle_epi8(_M(uvOdd), _M(shuff1));							// PSHUFB		1 1 3   0.5 1 2
	// C1 Sb 0 0	C3 Sb 0 0	C5 Sb 0 0	C7 Sb 0 0

	_M(uvEven) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[2], _M(uvRCoeffs));// PMADDWD		9 8 2 2
	// U2*0 + V2*204	U4*0 + V4*204	U6*0 + V6*204	U8*0 + V8*204
	// C2			C4				C6				C8

	// shift right by 7 to account for left shift by 7 of coefficients
	_M(uvEven) = _mm_srai_epi32(_M(uvEven), 7);										// PSRAD		2	2
	// C2 Sb Sb Sb	C4 Sb Sb Sb	C6 Sb Sb Sb	C8 Sb Sb Sb (4 * 32-bit values)

	// Move these values to the hi word and right shift by 7 to account for 7-bit left shift
	// of coefficients: 16 - 7 = 9 - we need to left shift by 9 bits.
	_M(uvEven) = _mm_shuffle_epi8(_M(uvEven), _M(shuff2));							// PSHUFB		1 1 3   0.5 1 2
	// 0 C2			0 C4		0 C6		0 C8

	_M(uvEven) = _mm_or_si128(_M(uvEven), _M(uvOdd));								// POR			2	2
	// C1 C2		C3 C4		C5 C6		C7 C8

	out_3_v16i_rgb_vectors[0] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(uvEven));	// PADDW	2	2



	//
	// G
	// U and V coeffs
	// -50, -104, -50, -104, -50, -104, -50, -104,
	_M(uvOdd) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[1], _M(uvGCoeffs));	// PMADDWD		9 8 2 2
	// C1			C3				C5				C7		(4 * 32-bits values)

	_M(uvOdd) = _mm_srai_epi32(_M(uvOdd), 7);										// PSRAD		2	2
	// C1 Sb Sb Sb	C3 Sb Sb Sb	C5 Sb Sb Sb	C7 Sb Sb Sb (4 * 32-bit values)

	_M(uvOdd) = _mm_shuffle_epi8(_M(uvOdd), _M(shuff1));							// PSHUFB		1 1 3   0.5 1 2
	// C1 0			C3  0		C5 0		C7 0

	_M(uvEven) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[2], _M(uvGCoeffs));// PMADDWD		9 8 2 2
	// U2*0 + V2*204	U4*0 + V4*204	U6*0 + V6*204	U8*0 + V8*204
	// C2			C4				C6				C8

	_M(uvEven) = _mm_srai_epi32(_M(uvEven), 7);										// PSRAD		2	2

	_M(uvEven) = _mm_shuffle_epi8(_M(uvEven), _M(shuff2));							// PSHUFB		1 1 3   0.5 1 2
	// 0 C2			0 C4		0 C6		0 C8

	_M(uvEven) = _mm_or_si128(_M(uvEven), _M(uvOdd));								// POR			2	2
	// C1 C2		C3 C4		C5 C6		C7 C8

	out_3_v16i_rgb_vectors[1] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(uvEven));	// PADDW	2	2



	//
	// B
	// U and V coeffs
	// 386, 0, 386, 0, 386, 0, 386, 0,
	_M(uvOdd) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[1], _M(uvBCoeffs));	// PMADDWD		9 8 2 2
	// C1			C3				C5				C7		(4 * 32-bits values)

	_M(uvOdd) = _mm_srai_epi32(_M(uvOdd), 7);										// PSRAD		2	2
	// C1 Sb Sb Sb	C3 Sb Sb Sb	C5 Sb Sb Sb	C7 Sb Sb Sb (4 * 32-bit values)

	_M(uvOdd) = _mm_shuffle_epi8(_M(uvOdd), _M(shuff1));							// PSHUFB		1 1 3   0.5 1 2
	// C1 0			C3  0		C5 0		C7 0

	_M(uvEven) = _mm_madd_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[2], _M(uvBCoeffs));// PMADDWD		9 8 2 2
	// U2*0 + V2*204	U4*0 + V4*204	U6*0 + V6*204	U8*0 + V8*204

	_M(uvEven) = _mm_srai_epi32(_M(uvEven), 7);										// PSRAD		2	2

	_M(uvEven) = _mm_shuffle_epi8(_M(uvEven), _M(shuff2));							// PSHUFB		1 1 3   0.5 1 2
	// 0 C2			0 C4		0 C6		0 C8

	_M(uvEven) = _mm_or_si128(_M(uvEven), _M(uvOdd));								// POR			2	2
	// C1 C2		C3 C4		C5 C6		C7 C8

	out_3_v16i_rgb_vectors[2] = _mm_add_epi16(in_3_v16i_y_uvOdd_uvEven_vectors[0], _M(uvEven));	// PADDW	2	2
};


#endif /* YUV_TO_RGB_CONVERT_BT_601_H_ */

