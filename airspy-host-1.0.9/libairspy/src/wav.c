/* module for dealing with .wav files */
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "wav.h"

#define DATE_TIME_MAX_LEN (32)

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

union semun {                   /* Used in calls to semctl() */
    int                 val;
    struct semid_ds *   buf;
    unsigned short *    array;
#if defined(__linux__)
    struct seminfo *    __buf;
#endif
};

/* IPC Keys */
#define IPC_Path "/tmp"
#define IPC_Proj ((int) 'L') /* as in LTO */

/* WAV default values */
uint16_t wav_format_tag=1; /* PCM8 or PCM16 */
uint16_t wav_nb_channels=2;
uint32_t wav_sample_per_sec;
uint16_t wav_nb_byte_per_sample=2;
uint16_t wav_nb_bits_per_sample=16;

/* wav globals */
t_wav_file_hdr wave_file_hdr = 
{
	/* t_WAVRIFF_hdr */
	{
		{ 'R', 'I', 'F', 'F' }, /* groupID */
		0, /* size to update later */
		{ 'W', 'A', 'V', 'E' }
	},
	/* t_FormatChunk */
	{
		{ 'f', 'm', 't', ' ' }, /* char		chunkID[4];  */
		16, /* uint32_t chunkSize; */
		0, /* uint16_t wFormatTag; to update later */
		0, /* uint16_t wChannels; to update later */
		0, /* uint32_t dwSamplesPerSec; Freq Hz sampling to update later */
		0, /* uint32_t dwAvgBytesPerSec; to update later */
		0, /* uint16_t wBlockAlign; to update later */
		0, /* uint16_t wBitsPerSample; to update later  */
	},
	/* t_DataChunk */
	{
		{ 'd', 'a', 't', 'a' }, /* char chunkID[4]; */
		0, /* uint32_t	chunkSize; to update later */
	}
};
int wav_fd = -1;
char wav_dirname[PATH_MAX], wav_filename[PATH_MAX];
uint32_t wav_freq_hz;


/*ipc globals */
int rxd_semid;
airspy_ipc_t *rxd_ipc;

int ipc_init()
{
    /* returns 0 upon success */

    key_t ipc_key;
    int shmid, sid;
    union semun arg;

    /* get the ipc key */
    if ((ipc_key = ftok(IPC_Path, IPC_Proj)) == (key_t) -1) errExit("ftok");

    shmid = shmget(ipc_key, sizeof(airspy_ipc_t), IPC_CREAT|00660);
    if (shmid == -1) errExit("shmget");

    rxd_ipc = (airspy_ipc_t*)shmat(shmid, NULL, 0);
    if (rxd_ipc == (airspy_ipc_t*)(-1))
    {
        errExit("shmat");
    }
    /* zero out the segment in case left over from previous instance */
    memset(rxd_ipc, 0, sizeof(airspy_ipc_t));

    rxd_ipc->rx_pid = getpid();

    /* create & set the ipc semaphore */
    sid = semget(ipc_key, 1, IPC_CREAT|00660);
    if (sid == -1) errExit("semget");
    arg.val = 0;
    if (semctl(sid, 0, SETVAL, arg) == -1)
        errExit("semctl");
    rxd_semid = sid;

    return 0;
}

wav_handle_t wav_init(enum wav_proc_type proc_type,
             uint16_t fmt,
             uint32_t freq_hz,
             uint16_t nchan,
             uint32_t sample_per_sec,
             uint16_t byte_per_sample,
             uint16_t bits_per_sample)
{

    /* Wav Format Chunk */
    wave_file_hdr.fmt_chunk.wFormatTag = fmt;
    wave_file_hdr.fmt_chunk.wChannels = nchan;
    wave_file_hdr.fmt_chunk.dwSamplesPerSec = sample_per_sec;
    wave_file_hdr.fmt_chunk.dwAvgBytesPerSec = sample_per_sec * byte_per_sample;
    wave_file_hdr.fmt_chunk.wBlockAlign = nchan * (bits_per_sample / 8);
    wave_file_hdr.fmt_chunk.wBitsPerSample = bits_per_sample;
    wav_freq_hz = freq_hz;

    if (proc_type == WAV_PROC_TYPE_DAEMON){
        /* set up ipc objects */
        if ( ipc_init() != 0) {
            fprintf(stderr, "unable to set up IPC channel\n");
            return WAV_ERROR_VAL;
        } 
    }

    return WAV_HANDLE_INACTIVE_VAL; //inactive but valid
}

wav_handle_t wav_creat(char *dirname, char *rootname)
{
    char path[PATH_MAX];
    time_t rawtime;
    struct tm * timeinfo;
    char date_time[DATE_TIME_MAX_LEN];
    DIR *dir;

    /* create the directory if needed */
    dir = opendir(dirname);
    if (!dir) {
        if (errno != ENOENT) {
            perror("Directory exists");
            return WAV_ERROR_VAL;
        }
        if (mkdir(dirname, 0777) < 0 ) {
            perror("mkdir");
            return WAV_ERROR_VAL;
        }
    }
    strcpy(wav_dirname, dirname);

    /* make the file name */
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    /* file name format: <dirname>/<rootname>_<date_time>.wav */
    strftime(date_time, DATE_TIME_MAX_LEN, "%Y%m%d_%H%M%S", timeinfo);
    snprintf(wav_filename, PATH_MAX, "%s_%sZ_%ukHz_IQ.wav",rootname, date_time,(uint32_t)(wav_freq_hz/(1000ull)));

    wav_fd = open(wav_dirname, __O_TMPFILE|O_RDWR, 0666);
    if (wav_fd < 0) {
        perror("open");
        return WAV_ERROR_VAL;
    }

    /* write the dummy wave header */
    if (write(wav_fd, &wave_file_hdr, sizeof(t_wav_file_hdr)) != sizeof(t_wav_file_hdr)){
        perror("Wav file hdr write");
        return WAV_ERROR_VAL;
    }

    return ((wav_handle_t)(1));
}

wav_handle_t wav_close(wav_handle_t hndl)
{
    char old_path[PATH_MAX], new_path[PATH_MAX];
    off_t file_pos;
    t_wav_file_hdr wav_hdr;

    if (wav_fd < 0) return 0;

    /* pick up the wave file parameters */
    memcpy(&wav_hdr, &wave_file_hdr, sizeof(t_wav_file_hdr));

    /* Get size of file and update necessary header members */
    file_pos = lseek(wav_fd, 0, SEEK_CUR);
    wav_hdr.hdr.size = file_pos - 8;
    wav_hdr.data_chunk.chunkSize = file_pos - sizeof(t_wav_file_hdr);

    /* rewrite header with updated data */
    lseek(wav_fd, 0, SEEK_SET);
    if (write(wav_fd, &wav_hdr,sizeof(t_wav_file_hdr)) != sizeof(t_wav_file_hdr)) errExit("wav hdr");

    /* make the file visible */
    snprintf(old_path, PATH_MAX, "/proc/self/fd/%d", wav_fd);
    snprintf(new_path, PATH_MAX,  "%s/%s", wav_dirname, wav_filename);
    if (linkat(AT_FDCWD, old_path, AT_FDCWD, new_path, AT_SYMLINK_FOLLOW) < 0) {
        perror("linkat");
        return WAV_ERROR_VAL;
    }

    close(wav_fd);
    wav_fd = -1;

    return WAV_HANDLE_INACTIVE_VAL;
}

int wav_write(enum wav_proc_type proc_type, void* rx_buf, u_int32_t bytes_to_write)
{
    ssize_t bytes_written;
    struct sembuf sop;
    static int32_t blocks_remaining;
    static wav_handle_t current_handle;

    /* use bytes_written == -1 as general error return value */
    if (proc_type == WAV_PROC_TYPE_NORMAL)
    {
        bytes_written = write(wav_fd, rx_buf, bytes_to_write);
    } else
    {
        /* wait for semaphore */
        sop.sem_num = 0;
        sop.sem_op = 0; //wait for zero
        sop.sem_flg = 0;
        if (semop(rxd_semid, &sop, 1) == -1) {
            errExit("semop1");
        }

        if (rxd_ipc->nblocks > 0) /* we need to write data */
        {
            if (wav_fd < 0){
                current_handle = wav_creat(rxd_ipc->dirname, rxd_ipc->filename);
                if (wav_error(current_handle)) return -1;
                blocks_remaining = rxd_ipc->nblocks;
            }

            bytes_written = write(wav_fd, rx_buf, bytes_to_write);
            blocks_remaining--;

            if (blocks_remaining == 0){
                current_handle = wav_close(current_handle);
                if wav_error(current_handle) bytes_written = -1;
            }
        } else /* pretend we did something useful */
        {
            if (wav_isactive(current_handle)) current_handle = wav_close(current_handle);
            bytes_written = bytes_to_write;
        } 
    }
    return bytes_written;
}
