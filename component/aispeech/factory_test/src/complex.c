#include "complex.h"
#include <stdio.h>

complex complex_multiply(complex a, complex b)
{
	complex retval;
	retval.real = a.real * b.real -a.imag *b.imag;
	retval.imag = a.real * b.imag + a.imag * b.real;
	return retval;
}

complex complex_div_double(complex a, double b)
{
	complex retval;
	retval.real = a.real / b;
	retval.imag = a.imag / b;
	return retval;
}

complex complex_multiply_double(complex a, double b)
{
	complex retval;
	retval.real = a.real * b;
	retval.imag = a.imag * b;
	return retval;
}

complex complex_add(complex a, complex b)
{
	complex retval;
	retval.real = a.real + b.real;
	retval.imag = a.imag + b.imag;
	return retval;
}

complex complex_create(double a, double b)
{
	complex retval;
	retval.real = a;
	retval.imag = b;
	return retval;
}

complex complex_minus(complex a, complex b)
{
	complex retval;
	retval.real = a.real - b.real;
	retval.imag = a.imag - b.imag;
	return retval;
}

void complex_print(complex a)
{
	printf("%f + %fi\n", a.real, a.imag);
}

void complex_convert_array(short* in_real, complex *out, int size)
{
	int i;
	for(i = 0; i < size; ++i)
	{
		out[i] = complex_create(in_real[i], 0);
	}
}

void complex_convert_array2short(complex *in, short *out, int size)
{
	int i;
	for(i = 0; i < size; ++i)
	{
		out[i] = in[i].real;
	}
}

void complex_array_operation(complex *a, int size, complex (*p)(complex, double), double scale)
{
	int i;
	for(i = 0; i < size; ++i)
	{
		a[i] = p(a[i], scale);
	}
}

double complex_power(complex a)
{
	return a.real * a.real + a.imag * a.imag;
}

complex complex_conjugate(complex a)
{
	a.imag = - a.imag;
	return a; 
}
/*
int main()
{
	complex_print(complex_multiply(complex_create(2,3), complex_create(4,5)));	
}
*/
