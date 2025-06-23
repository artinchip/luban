// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Author: weijie.ding <weijie.ding@artinchip.com>
 */
#include "goertzel.h"

#define FFTSIZE                 480
#define Q14 			16384
#define Q12			4096
#define Q16			65535

extern int16_t audio_rx_data[480];

double logarithm(unsigned int i, double temp, double base, double num,
		 unsigned int acc)
{
	if(acc == 1)
		return 0;
	if(num >= base) {
		return 1 + logarithm(0, 1, base, num / base, acc - 1);
	} else {
		while(i++ < base)
			temp *= num;
		return (1/base) * logarithm(0, 1, base, temp, acc - 1);
	}
}

double log10(double x)
{
	return logarithm(0, 1.0, 10.0, x, 100);
}

double goertzel(short samples[], double freq, int N)
{
	int i;
	double s_prev = 0.0;
	double s_prev2 = 0.0;
	double coeff, power, s;
	double cosval[6] = {1.9957178, 1.9828897, 1.9318517, 1.847759,
			    1.9832297, 1.9825463};

	if(freq == 500)
		coeff = cosval[0];
	else if(freq == 1000)
		coeff = cosval[1];
	else if(freq == 2000)
		coeff = cosval[2];
	else if(freq == 3000)
		coeff = cosval[3];
	else if(freq == 990)
		coeff = cosval[4];
	else if(freq == 1010)
		coeff = cosval[5];
	else
		coeff = 0;

	for(i = 0; i < N; i++) {
		s = samples[i] + coeff * s_prev - s_prev2;
		s_prev2 = s_prev;
		s_prev = s;
	}

	power = s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2;

	return power;
}

int goertzel_convert(short *fSinedata)
{
	int i, ret=0;
	double fVpp=0.0;
	int FVPP=0;

	float frequency_row[6] = {500, 1000, 2000, 3000, 990, 1010};
	float magnitude_row[6];
	float snr0 = 0.0, snr1 = 0.0, snr2 = 0.0, rate3 = 0.0, rate4 = 0.0;
	int Snr0 = 0, Snr1 = 0, Snr2 = 0, Rate3 = 0, Rate4 = 0;
	short fRmax, fRmin;

	//Get the max and min sine data
	fRmax = 0x800;
	fRmin = 0x7FF;
	for(i = 0; i < FFTSIZE; i++) {
		if(fSinedata[i] > fRmax)
			fRmax = fSinedata[i];
		if(fSinedata[i] < fRmin)
			fRmin = fSinedata[i];
	}

	printf("fRmax: %d, fRmin: %d\n", fRmax, fRmin);

	fVpp = (double)fRmax / Q12 - (double)fRmin / Q12;
	FVPP = fVpp * 1000;
	printf("fVpp=%dmV!\n", FVPP);

	if((fVpp < 0.8) || (fVpp > 2.0)) {
		ret = -1;
		goto __exit;
	}

	//Get the Freq and THD
	for(i = 0; i < 6; i++)
		magnitude_row[i] = \
				goertzel(fSinedata, frequency_row[i], FFTSIZE);

	snr0 = 20 * log10(magnitude_row[1] / magnitude_row[0]);
	snr1 = 20 * log10(magnitude_row[1] / magnitude_row[2]);
	snr2 = 20 * log10(magnitude_row[1] / magnitude_row[3]);
	rate3 = magnitude_row[1] / magnitude_row[4];
	rate4 = magnitude_row[1] / magnitude_row[5];
	Snr0 = snr0 * 1000;
	Snr1 = snr1 * 1000;
	Snr2 = snr2 * 1000;
	Rate3 = rate3 * 1000;
	Rate4 = rate4 * 1000;

	printf("snr0 * 1000 = %d!\n", Snr0);
	printf("snr1 * 1000 = %d!\n", Snr1);
	printf("snr2 * 1000 = %d!\n", Snr2);
	printf("rate3 * 1000 = %d!\n", Rate3);
	printf("rate4 * 1000 = %d!\n", Rate4);

	if((Snr0 < 20000) || (Snr1 < 20000) || (Snr2 < 20000)) {
		ret = -2;
		goto __exit;
	}

	if(rate3 < 1 || rate4 < 1)
		ret = -3;

__exit:
	return ret;
}

int goertzel_test(void)
{
	unsigned int i;
	int ret=0;
	static short fAdcdt[FFTSIZE];

	int16_t *test_data = (int16_t *)audio_rx_data;

	for(i = 0; i < FFTSIZE; i++)
		fAdcdt[i] = test_data[i];

	ret = goertzel_convert(fAdcdt);

	return ret;
}

