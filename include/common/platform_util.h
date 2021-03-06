/*
 * platform_util.h
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

#ifndef PLATFORM_UTIL_H
#define PLATFORM_UTIL_H

// Platform detection
// If we are building on Windows, assume we are building for Intel
#if defined(WIN32) || defined(__i386__) || defined(__x86_64__)
#define __INTEL_CPU__	1
#endif

#ifdef __INTEL_CPU__
#include <emmintrin.h>
#else // __INTEL_CPU__
typedef long long __m128i __attribute__ ((__vector_size__ (16), __may_alias__));
#endif	// __INTEL__CPU__




#if defined(__linux__) || defined(__APPLE__)

/*
 * We need the following because Visual Sudio cant handle assignment to a __m128i
 * variable. So instead, we have to declare the variable as a 16-byte aligned
 * array of __int64 and we have to cast it to __m128i when used. This is SHIT !!!!!
 * To add to the confusion, VS also requires all aligned variables to be declared at 
 * the top of a function.
 * http://permalink.gmane.org/gmane.comp.lib.cairo/14414
 */
#define CONST_M128I_ARRAY(var, num_elem)				const __m128i var[(num_elem)]
#define M128I_ARRAY_VAL(var, ignored, ...)				__m128i var[]={__VA_ARGS__}
#define CONST_M128I(var, val64_1, val64_2)				static const __m128i var = { val64_1, val64_2 }
#define M128I(var, val64_1, val64_2)					__m128i var = { val64_1, val64_2 }
// Use the macros below to access one __m128i element declared with the above macros.
#define	_M(var)											(var)
#define	_AM(var)										(&(var))

#else

/*
 * Here starts the Microsoft __m128i awesomeness
 */

#define CONST_M128I_ARRAY(var, num_elem)				__declspec(align(16)) const __int64 var[(num_elem)][2]
#define M128I_ARRAY_VAL(var, num_elem, ...)				__declspec(align(16)) __int64 var[(num_elem)][2]={__VA_ARGS__}
#define CONST_M128I(var, val64_1, val64_2)				__declspec(align(16)) static const __int64 var[] = { (val64_1), (val64_2) }
#define M128I(var, val64_1, val64_2)					__declspec(align(16)) __int64 var[] = { (val64_1), (val64_2)}
// Use the macros below to access a variable declared with the above macros.
#define	_M(var)											(*((__m128i *)(var)))	// dereference one element
#define _AM(var)										((__m128i *)(&(var)))		// Address of one element

#endif


#endif	// #ifndef PLATFORM_UTIL_H
