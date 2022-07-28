#include "spectrum_calc.h"
#include "fourier.h"
#include "complex.h"
#include "math.h"
//#include "wavfile.h"

#if 0
void spectrum_calc(FILE * file, double freq_mean[], int channel_num, int channel_index, int start_pos, int end_pos, int frame_size)
{
	static short frame_buff[1024];
	static complex tmp[1024];
	int i, count = 0;
	
	int FFT_bins = frame_size/2 + 1;

	for(i = 0; i < FFT_bins; ++i)
	{
		freq_mean[i] = 0;
	}

	while(end_pos - start_pos + 1 >= frame_size)
	{
		//read the data of the channel to frame_buff.
		wavfile_read_channel(file, frame_buff, channel_num, channel_index, start_pos, frame_size);

		//convert the frame_buff to complex type in order to perform the fft 
		complex_convert_array(frame_buff, tmp, frame_size);

		FFT(tmp, tmp, frame_size);


		for(i = 0; i < FFT_bins; ++i)
		{
			freq_mean[i] += sqrt(complex_power(tmp[i])) / frame_size;
		}

		start_pos += frame_size;
		++ count;
	}
	
	for(i = 0; i < FFT_bins; ++i)
	{
		freq_mean[i] = pow(freq_mean[i] / count, 2);
	}
}
#endif

short wav_read_pos(short* file, int channel_num,int channel_index, int pos)
{
	short value;
	long index = 0 + (pos * channel_num + channel_index);
	value = file[index];
	return value;
}

void wav_read_channel(short* file, short data[], int channel_num, int channel_index, int start_pos, int length)
{
	int i;
	for(i = 0; i < length; ++i)
	{
		data[i] = wav_read_pos(file, channel_num, channel_index, start_pos + i);
		//printf("wav pos %d\n", data[i]);
	}
}

void spectrum_calc_feed(short* data, double freq_mean[], int channel_num, int channel_index, int start_pos, int end_pos, int frame_size)
{
	static short frame_buff[1024];
	static complex tmp[1024];
	int i, count = 0;

	int FFT_bins = frame_size/2 + 1;

	for(i = 0; i < FFT_bins; ++i)
	{
		freq_mean[i] = 0;
	}

	while(end_pos - start_pos + 1 >= frame_size)
	{
		//read the data of the channel to frame_buff.
		wav_read_channel(data, frame_buff, channel_num, channel_index, start_pos, frame_size);

		//convert the frame_buff to complex type in order to perform the fft
		complex_convert_array(frame_buff, tmp, frame_size);

		FFT(tmp, tmp, frame_size);


		for(i = 0; i < FFT_bins; ++i)
		{
			freq_mean[i] += sqrt(complex_power(tmp[i])) / frame_size;
		}

		start_pos += frame_size;
		++ count;
	}

	for(i = 0; i < FFT_bins; ++i)
	{
		freq_mean[i] = pow(freq_mean[i] / count, 2);
	}
}

double spectrum_thd(double spec[], int Fs, int frame_size)
{
    	// each bin's frequency = Fs/fft_size
	float bin_res = (float)Fs/ frame_size;
	int i;
	int index_tmp;
	double tmp = 0;
	
	// sum the square of 1kHz's harmonic(2kHz: 3kHz: to ...7kHz: )together
	for(i = 2; i <= 7; ++i)
	{
		index_tmp = 1000*i/ bin_res;
		tmp += spec[index_tmp];
	}
	
	index_tmp = 1000/bin_res;
	return sqrt(tmp / spec[index_tmp]);
}
