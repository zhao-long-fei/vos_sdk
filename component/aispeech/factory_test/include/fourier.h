#ifndef _FOURIER_H_
#define _FOURIER_H_
#define PI 3.14159265358979323846  
typedef struct complex complex;

void FFT(complex*,complex *,int nfft);  
void IFFT(complex*,complex *,int nfft); //inverse FFT  

#endif
