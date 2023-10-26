/**
 *	\file	polyfill.h
 *	\brief	古い一部環境にてINFINITYやNANを扱えていなかったのでその対策
 */
#ifndef	NMJSON_POLYFILL_H_
#define	NMJSON_POLYFILL_H_

#include <math.h>

#ifndef	INFINITY
	#warning "INFINITY" is not defined.
	#define INFINITY (1.0 / 0.0)
#endif	/* !INFINITY */

#ifndef isinf
	#warning "isinf(x)" is not defined.
	#define isinf(x) ((x != 0.0) && (x != -0.0) && (x == x * 2.0))
#endif	/* !isinf */

#ifndef NAN
	#warning "NAN" is not defined.
	#define NAN (0.0 / 0.0)
#endif	/* !NAN */

#ifndef isnan
	#warning "isnan(x)" is not defined.
	#define isnan(x) (x != x)
#endif	/* !isnan */

#endif	/* !NMJSON_POLYFILL_H_ */
