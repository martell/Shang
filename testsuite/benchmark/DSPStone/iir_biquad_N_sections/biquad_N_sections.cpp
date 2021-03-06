/*
 *  benchmark program  : biquad_N_sections.c
 *
 *  benchmark suite    : DSP-kernel
 *
 *  description        : benchmarking of an iir biquad (N sections)
 *                       
 *	The equations of each biquad section filter are:
 *       w(n) =    x(n) - ai1*w(n-1) - ai2*w(n-2)
 *       y(n) = b0*w(n) + bi1*w(n-1) + bi2*w(n-2)
 *
 * Biquads are sequentally positioned. Input sample for biquad i is
 * xi-1(n). Output sample for biquad i is xi(n). 
 * System input sample is x0(n). System output sample is xN(n) = y(n) 
 * for N biquads. 
 * 
 * Each section performs following filtering (biquad i) : 
 * 
 *		              wi(n)
 *   xi-1(n) ---(-)---------->-|->---bi0---(+)-------> xi(n)
 *               A             |            A
 *               |           |1/z|          |
 *               |             | wi(n-1)    |
 *               |             v            |
 *               |-<--ai1----<-|->---bi1-->-|
 *               |             |            |
 *               |           |1/z|          |
 *               |             | wi(n-2)    |
 *               |             v            |
 *               |-<--ai2----<--->---bi2-->-|
 * 
 *     The values wi(n-1) and wi(n-2) are stored in wi1 and wi2
 * 
 *                                              
 *  reference code     : 
 *
 *  func. verification : from separate computation
 *
 *  organization       : Aachen University of Technology - IS2
 *                     : DSP Tools Group
 *                     : phone   : +49(241)807887
 *                     : fax     : +49(241)8888195
 *                     : e-mail  : zivojnov@ert.rwth-aachen.de
 *
 *  author             : Juan Martinez Velarde
 *
 *  history            : 24-3-1994 C code creation and profiling (Martinez Velarde)
 *
 *                     $Author: schraut $
 *                     $Date: 1995/01/30 07:40:55 $
 *                     $Revision: 1.2 $
 */


#define STORAGE_CLASS register
#define TYPE int

#define NumberOfSections 4
#ifdef __cplusplus
extern "C" {
#endif
#include<stdio.h>
TYPE biquad_N_sections(TYPE x,TYPE *ptr_coeff,TYPE *ptr_wi1,TYPE *ptr_wi2){
	STORAGE_CLASS TYPE w ,f;
	TYPE y = x ; 
  
	for (f = 0 ; f < NumberOfSections ; f++)
	{
		w  = y -  *ptr_coeff++ * *ptr_wi1 ; 
		w -= *ptr_coeff++ * *ptr_wi2 ; 
      
		y  = *ptr_coeff++ *  w ; 
		y += *ptr_coeff++ * *ptr_wi1 ; 
		y += *ptr_coeff++ * *ptr_wi2 ; 
      
		*ptr_wi2++ = *ptr_wi1; 
		*ptr_wi1++ = w ;       

		ptr_wi2++ ;
		ptr_wi1++ ; 
	}
  
  return (y);
}
#ifdef __cplusplus
}
#endif

TYPE pin_down(TYPE x, TYPE coefficients[], TYPE wi[])
{
  int f; 

  for (f = 0 ; f < 5*NumberOfSections; f++)
    coefficients[f] = 7 ; 
  
  for (f = 0 ; f < 2*NumberOfSections; f++)
    wi[f] = 0 ; 
  
  return ((TYPE) 1) ;
}


TYPE main()
{
  //TYPE w, f ; 
  STORAGE_CLASS TYPE *ptr_coeff, *ptr_wi1, *ptr_wi2 ; 

  STORAGE_CLASS TYPE wi[2*NumberOfSections] ; 

  static TYPE coefficients[5*NumberOfSections]; 
  static TYPE x,y ; 

  ptr_coeff = &coefficients[0] ;
  ptr_wi1 = &wi[0] ; 
  ptr_wi2 = &wi[1] ;
  
  x = pin_down(x, coefficients, wi) ;

  y = biquad_N_sections(x,ptr_coeff,ptr_wi1,ptr_wi2);
  
  printf("y:%d\n",y); 
  
  pin_down(y,coefficients,wi) ; 

  return(0) ; 
}

