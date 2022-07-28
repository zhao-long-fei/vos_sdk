#ifndef _SPECTRUM_CALC_
#define _SPECTRUM_CALC_
#include <stdio.h>

/*
	compute the averaged spectrum of a channel.
	The function will get the data frame by frame from the specifiled file.
	input:
		file: the file pointer of the input file,
		channel_num: the number of channels in the FILE.
		channel_index: the index of channels. Started from 0.
		start_pos: the start position for the computation.
		end_pos: the end position of the computation.
		frame_size: the size of a frame
	output:
		freq_mean: the output array of averaged spectrum.

*/
void spectrum_calc(FILE * file, double freq_mean[], int channel_num, int channel_index, int start_pos, int end_pos, int frame_size);

/*
	compute the averaged spectrum of a channel.
	The function will get the data frame by frame from the specifiled file.
	input:
		data: the file pointer of the input file,
		channel_num: the number of channels in the FILE.
		channel_index: the index of channels. Started from 0.
		start_pos: the start position for the computation.
		end_pos: the end position of the computation.
		frame_size: the size of a frame
	output:
		freq_mean: the output array of averaged spectrum.

*/
void spectrum_calc_feed(short* data, double freq_mean[], int channel_num, int channel_index, int start_pos, int end_pos, int frame_size);

/*
	compute the total harmonic distortion for a spectrum.
	input:
		spec: the avaraged spectrum
		Fs: sample rate
		frame_size: the size of a frame.
	output:
		total harmonic distortion
*/
double spectrum_thd(double spec[], int Fs, int frame_size);
#endif
