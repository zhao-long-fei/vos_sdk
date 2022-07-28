#ifndef _COMPLEX_H_ 
#define _COMPLEX_H_

typedef struct complex{
	double real;
	double imag;
}complex;

//complex multiplication
complex complex_multiply(complex a, complex b);
	
//complex num divided by a double num
complex complex_div_double(complex a, double b);

//complex num multiplied by a double num
complex complex_multiply_double(complex a, double b);

//create complex num from primitive real and imaginary num
complex complex_create(double a, double b);

//complex addition
complex complex_add(complex a, complex b);

//complex substraction
complex complex_minus(complex a, complex b);

//print complex num in the format of real + imaginary i
void complex_print(complex a);

//convert the entire short type array to a complex type array
void complex_convert_array(short * in_real, complex * out, int size);

//extract the real part of each element to an short type array
void complex_convert_array2short(complex * in, short * out, int size);
 
//perform an operation to each element of a complex type array
void complex_array_operation(complex *a, int size, complex (*p)(complex, double), double scale);

//compute the power of a complex number
double complex_power(complex a);

//complex conjugation 
complex complex_conjugate(complex a);
#endif
