#include <stdio.h>  
#include <stdlib.h>  
#include <math.h>  
#include "complex.h"
#include "fourier.h"

static void common_fft(complex* x, complex *y,int nfft,int isign);

void FFT(complex* x, complex *y,int nfft)  
{  
	common_fft(x,y,nfft,1);  
}  
  
void IFFT(complex* x,complex *y,int nfft)  
{  
    int i;  

    common_fft(x,y,nfft,-1);  
  
    for(i=0;i<nfft;i++)  
    {  
        y[i].real /= nfft;  
        y[i].imag /= nfft;  
    }  
}  
  
/* fft kernel */  
/* isign: 1 for FFT , -1 for IFFT */  
static void common_fft(complex* x, complex *y,int nfft,int isign)  
{  
    int i,j=0,k;  
    complex t;  
  
    for(i=0;i<nfft -1;i++)  
    {  
        if(i<j)  
        {  
	    t = x[i];	
	    y[i] = x[j];
	    y[j] = t;

	    //y[i] = x[j];
	    //y[j] = x[i];
        }else if(i == j){
		y[i] = x[i];	
	}

    	k= (nfft>>1);  
        while(k<=j)  
        {  
            j-=k;  
            k/=2;  
        }  
        j+=k;  
    }  
	y[nfft-1] = x[nfft-1];

    int stage,le,lei,ip;  
    complex u,w;  
     j= nfft;  
     for(stage=1;(j=j/2)!=1;stage++); //calculate stage,which represents  butterfly stages  
      
    for(k=1;k<=stage;k++)  
    {  
        le=2<<(k-1);  
        lei= (le>>1);  
        u.real=1.0;// u,butterfly factor initial value  
        u.imag=0.0;  
        w.real=cos(PI/lei*isign);  
        w.imag= -sin(PI/lei*isign);  
        for(j=0;j<=lei-1;j++)  
        {  
            for(i=j;i<=nfft-1;i+=le)  
            {  
                ip = i+lei;  
                t = complex_multiply(y[ip],u);  
		y[ip] = complex_minus(y[i], t);
		y[i] = complex_add(y[i], t);
            }  
            u=complex_multiply(u,w);  
        }  
    }  
}
/*
int main(int argc,char* argv[]) {  
    int i;  
    int Nx;   
    int NFFT;  
    complex  *x, *y;  
  
    Nx=4;  
    printf("Nx = %d\n",Nx);  
      
    // caculate NFFT as the next higer power of 2 >=Nx/  
    NFFT = (int)pow(2.0,ceil(log((double)Nx)/log(2.0)));  
    printf("NFFT = %d \n",NFFT);  
  
    // allocate memory for NFFT complex numbers/  
    x=(complex*)malloc(NFFT*sizeof(complex));  
    y=(complex*)malloc(NFFT*sizeof(complex));  
  
    // input test data/  
    for(i=0;i<Nx;i++)  
    {  
        x[i].real=155+i;  
        x[i].imag=0.0;  
    }  
    for(i=Nx;i<NFFT;i++)  
    {  
        x[i].real=0.0;  
        x[i].imag=0.0;  
    }  
  
    for(i=0;i<NFFT;i++)  
    {  
	printf("x[%d] = ", i);
	complex_print(x[i]);
    }  
      
    // caculate FFT  /  
    FFT(x,y,NFFT);  
  
    printf("\nFFT:\n");  
    for(i=0;i<NFFT;i++)  
    {  
	printf("x[%d] = ", i);
	complex_print(y[i]);
    }  
  
    // caculate IFFT/  
    IFFT(y,x,NFFT);  
    printf("\ncomplex sequence reconstructed by IFFT:\n");  
    for(i=0;i<NFFT;i++)  
    {  
	printf("x[%d] = ", i);
	complex_print(x[i]);
    }  
      
    return 0;  
}  
*/
