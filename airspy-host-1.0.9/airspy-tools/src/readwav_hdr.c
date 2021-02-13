
#include <airspy.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

typedef struct 
{
		char groupID[4]; /* 'RIFF' */
		uint32_t size; /* File size + 8bytes */
		char riffType[4]; /* 'WAVE'*/
} t_WAVRIFF_hdr;

typedef struct {
	char chunkID[4]; /* 'fmt ' */
	uint32_t chunkSize; /* 16 fixed */

	uint16_t wFormatTag; /* 1=PCM8/16, 3=Float32 */
	uint16_t wChannels;
	uint32_t dwSamplesPerSec; /* Freq Hz sampling */
	uint32_t dwAvgBytesPerSec; /* Freq Hz sampling x 2 */
	uint16_t wBlockAlign;
	uint16_t wBitsPerSample;
} t_FormatChunk;

typedef struct 
{
		char chunkID[4]; /* 'data' */
		uint32_t chunkSize; /* Size of data in bytes */
	/* For IQ samples I(16 or 32bits) then Q(16 or 32bits), I, Q ... */
} t_DataChunk;

typedef struct
{
	t_WAVRIFF_hdr hdr;
	t_FormatChunk fmt_chunk;
	t_DataChunk data_chunk;
} t_wav_file_hdr;

#define member_size(type, member) sizeof(((type *)0)->member)
#define NFRAMES 32

void tohex(unsigned char * in, size_t insz, char * out, size_t outsz)
{
    unsigned char * pin = in;
    const char * hex = "0123456789ABCDEF";
    char * pout = out;
    for(; pin < in+insz; pout +=5, pin+=2){
        pout[0] = hex[(*pin>>4) & 0xF];
        pout[1] = hex[ *pin     & 0xF];
        pout[2] = hex[(*(pin+1)>>4) & 0xF];
        pout[3] = hex[ *(pin+1)     & 0xF];
        pout[4] = ' ';
        if (pout + 5 - out > outsz){
            /* Better to truncate output string than overflow buffer */
            /* it would be still better to either return a status */
            /* or ensure the target buffer is large enough and it never happen */
            break;
        }
    }
    pout[-1] = 0;
}

int main(int argc, char **argv)
{
    int fd;
    t_wav_file_hdr wav_hdr;
    char buf[1024], buf2[1024];
    int len, i, fpos;
    int16_t iq_data[2*NFRAMES], *pframe;


    memset(buf, 0, sizeof(buf));
    memset(buf2, 0, sizeof(buf2));
    memset(iq_data, 0x13, sizeof(iq_data));

    fd = open(argv[1], O_RDONLY);
    if (!fd) {
        fprintf(stderr,"No such file: %s\n", argv[1]);
        exit(-1);
    }

    if (read(fd, &wav_hdr, sizeof(t_wav_file_hdr)) != sizeof(t_wav_file_hdr))
    {
        fprintf(stderr,"File too short: %s\n", argv[1]);
        exit(-1);
    }

    len = sizeof(t_wav_file_hdr);
    printf("Header length: %d\n", len);

    /* Print the header */
    len = member_size(t_WAVRIFF_hdr, groupID);
    memcpy(buf, &wav_hdr.hdr.groupID, len);
    buf[len] = (char) '\0';

    len = member_size(t_WAVRIFF_hdr, riffType);
    memcpy(buf2, &wav_hdr.hdr.riffType, len);
    buf2[len]= '\0';

    printf("---- RIFF Hdr:\n");
    printf("\tgroupID: %s, size: %u, riffType: %s\n", buf, wav_hdr.hdr.size,buf2);

    printf("\n---- Format Chunk:\n");
    len = member_size(t_FormatChunk, chunkID);
    memcpy(buf2, &wav_hdr.fmt_chunk, len);
    buf2[len]= '\0';
    printf("\tchunkID: %s, chunkSize: %u, wFormatTag: %u, wChannels: %u, dwSamplesPerSec: %u, dwAvgBytesPerSec: %u, BlockAlign: %u, BitsPerSample: %u\n",
        buf2,
        wav_hdr.fmt_chunk.chunkSize,
        wav_hdr.fmt_chunk.wFormatTag,
        wav_hdr.fmt_chunk.wChannels,
        wav_hdr.fmt_chunk.dwSamplesPerSec,
        wav_hdr.fmt_chunk.dwAvgBytesPerSec,
        wav_hdr.fmt_chunk.wBlockAlign,
        wav_hdr.fmt_chunk.wBitsPerSample);

    printf("\n---- Data Chunk:\n");
    len = member_size(t_DataChunk, chunkID);
    memcpy(buf2, &wav_hdr.data_chunk, len);
    buf2[len]= '\0';
    printf("\tChunkID: %s, chunkSize: %u\n", buf2, wav_hdr.data_chunk.chunkSize);


    fpos = lseek(fd, 0, SEEK_CUR);
    len = read(fd, iq_data, sizeof(iq_data));

    printf("\n\tFirst few frames, read %d bytes, starting at offset: %d:\n", len, fpos);
    tohex((unsigned char*)iq_data, sizeof(iq_data), buf, sizeof(buf));
    printf("Byte string: %s\n", buf);


    pframe = iq_data;
    for (i=0; i < NFRAMES/2; i++){
        printf("%02d    0x%04x  0x%04x\n",i, *(uint16_t*)pframe, *(uint16_t*)(pframe+1));
        printf("%02d     %d     %d\n",i, *pframe, *(pframe+1));
        printf("\n");
        pframe += 2;
    }
    exit(0);



}