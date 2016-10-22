/*	$NetBSD: stdarg.h,v 1.12 1995/12/25 23:15:31 mycroft Exp $	*/

#ifndef JOS_INC_STDARG_H
#define	JOS_INC_STDARG_H

typedef __builtin_va_list va_list;

/*
 bocui comment
 __builtin_va_start, gcc builtin function
 Description
 Explanation:
 A function may be called with a varying number of arguments of varying types. 
 The include file <stdarg.h> declares a type va_list and 
 defines three macros for stepping through a list of arguments whose number and types are not known to the called function.
 The called function must declare an object of type va_list which is used by the macros va_start(), va_arg(), and va_end().
*/
#define va_start(ap, last) __builtin_va_start(ap, last)

#define va_arg(ap, type) __builtin_va_arg(ap, type)

#define va_end(ap) __builtin_va_end(ap)

#endif	/* !JOS_INC_STDARG_H */
