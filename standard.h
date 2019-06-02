

#ifndef STANDARD_H
#define STANDARD_H

/* Machine dependent typecasts */

typedef signed long	int32;	    /* signed 32-bit quantity */
typedef unsigned long	uint32;	    /* unsigned 32-bit quantity */
typedef signed short	int16;	    /* signed 16-bit quantity */
typedef unsigned short	uint16;	    /* unsigned 16-bit quantity */
typedef unsigned char	uint8;	    /* unsigned 8-bit quantity */
typedef signed char	int8;
typedef char		text;
typedef enum{false,true} bool;

typedef float		real32;		/* Floating point types */
typedef double		real64;

/* Limits for the Motorola FFP */
#define	REAL32_MIN	5.42101070E-20
#define REAL32_MAX	9.22337177E+18
#define	REAL64_MIN	5.42101070E-20
#define REAL64_MAX	9.22337177E+18

typedef	real64		real;		/* As used in program */
#define REAL_MIN	REAL64_MIN
#define REAL_MAX	REAL64_MAX

/*
#ifndef NULL
#define NULL	((void *)0)
#endif
*/

#define class 		typedef struct

#define failif(x)	if(false==(x)) return false

#define PI  3.141592654
#define PI2 6.283185307

#define SQR(x) ((x)*(x))

class COMPLEX {
real re,im;
} COMPLEX;

#endif
