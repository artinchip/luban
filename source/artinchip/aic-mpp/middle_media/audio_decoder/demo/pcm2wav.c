/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_audio_decoder interface
 */

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

struct wav_header {
	char chunk_id[4];/*RIFF*/
	unsigned int chunk_size;
	char format[4];/*WAVE*/
};

struct wav_fmt {
	char sub_chunk1_id[4];/*fmt*/
	unsigned int sub_chunk1_size;
	unsigned short audio_format;/*pcm=1*/
	unsigned short channels;
	unsigned int sample_rate;/**/
	unsigned int byte_rate;/* byte_rate = sample_rate*channels*bits_per_sample/8 */
	unsigned short block_align;/* channels*bits_per_sample/8  */
	unsigned short bits_per_sample;
};

struct wav_data {
	char sub_chunk2_id[4];/*data*/
	unsigned int sub_chunk2_size;
};

int pcm_to_wav(char *pcm_file,char *wav_file)
{
	struct wav_header header;
	struct wav_fmt fmt;
	struct wav_data data;
	unsigned int pcm_size = 0;
	int pcm_fd;
	int wave_fd;
	int ret = 0;
	char buffer[16*1024] ={0};
	int len;

	pcm_fd = open(pcm_file, O_RDONLY);
	if (pcm_fd < 0) {
		printf("failed to open input file %s", pcm_file);
		ret = -1;
		goto _EXIT0;
	}
	pcm_size = lseek(pcm_fd, 0, SEEK_END);
	lseek(pcm_fd, 0, SEEK_SET);

	wave_fd = open(wav_file, O_RDWR|O_CREAT);
	if (wave_fd < 0) {
		printf("failed to open input file %s", wav_file);
		ret = -1;
		goto _EXIT1;
	}

	/*header*/
	memset(&header,0x00,sizeof(struct wav_header));
	memcpy(header.chunk_id,"RIFF",4);
	memcpy(header.format,"WAVE",4);
	header.chunk_size = 4 + sizeof(struct wav_fmt) + sizeof(struct wav_data) + pcm_size;

	/*fmt*/
	memset(&fmt,0x00,sizeof(struct wav_fmt));
	memcpy(fmt.sub_chunk1_id,"fmt ",4);
	fmt.sub_chunk1_size = 16;
	fmt.audio_format = 1;
	fmt.channels = 1;
	fmt.sample_rate = 48000;
	fmt.byte_rate = 64000;
	fmt.block_align = 2;
	fmt.bits_per_sample = 16;

	/*data*/
	memset(&data,0x00,sizeof(struct wav_data));
	memcpy(data.sub_chunk2_id,"data",4);
	data.sub_chunk2_size = pcm_size;

	write(wave_fd,&header,sizeof(struct wav_header));
	write(wave_fd,&fmt,sizeof(struct wav_fmt));
	write(wave_fd,&data,sizeof(struct wav_data));

	while((len = read(pcm_fd, buffer,sizeof(buffer))) != 0){
		if(len < 0){
			ret =-1;
			printf("read error \n");
			break;
		}
		write(wave_fd,buffer,len);
	}
	close(wave_fd);
_EXIT1:
	close(pcm_fd);
_EXIT0:
	return ret;

}


