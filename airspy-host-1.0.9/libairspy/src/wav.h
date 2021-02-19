#ifdef _WIN32
	 #define ADD_EXPORTS
	 
	/* You should define ADD_EXPORTS *only* when building the DLL. */
	#ifdef ADD_EXPORTS
		#define ADDAPI __declspec(dllexport)
	#else
		#define ADDAPI __declspec(dllimport)
	#endif

	/* Define calling convention in one place, for convenience. */
	#define ADDCALL __cdecl

#else /* _WIN32 not defined. */

	/* Define with no value on non-Windows OSes. */
	#define ADDAPI
	#define ADDCALL

#endif

#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif

enum wav_proc_type{
    WAV_PROC_TYPE_DAEMON = 1,
    WAV_PROC_TYPE_NORMAL = 2

};


/* WAVE or RIFF WAVE file format containing data for AirSpy compatible with SDR# Wav IQ file */
typedef struct 
{
		char groupID[4]; /* 'RIFF' */
		uint32_t size; /* File size + 8bytes */
		char riffType[4]; /* 'WAVE'*/
} t_WAVRIFF_hdr;

#define FormatID "fmt "   /* chunkID for Format Chunk. NOTE: There is a space at the end of this ID. */

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

/* IPC Keys */
#define IPC_Path "/tmp"
#define IPC_Proj ((int) 'L') /* as in LTO */
typedef struct {
    pid_t rx_pid;
    int32_t nblocks;
    char dirname[FILENAME_MAX];
    char filename[FILENAME_MAX];
} airspy_ipc_t;

typedef int wav_handle_t; /* -2 iff error else index into wav table */
#define WAV_ERROR_VAL (wav_handle_t)(-2)
#define wav_error(hndl) ((hndl) == WAV_ERROR_VAL)
#define WAV_HANDLE_INACTIVE_VAL (wav_handle_t)(-1)
#define wav_isactive(hndl) ((hndl) != WAV_HANDLE_INACTIVE_VAL)

extern ADDAPI wav_handle_t ADDCALL wav_init(enum wav_proc_type proc_type,
                                    uint16_t fmt,
                                    uint32_t freq_hz,
                                    uint16_t nchan,
                                    uint32_t sample_per_sec,
                                    uint16_t byte_per_sample,
                                    uint16_t bits_per_sample);

extern ADDAPI wav_handle_t ADDCALL wav_open(char *dirname, char *rootname);
extern ADDAPI wav_handle_t ADDCALL wav_close(wav_handle_t);
extern ADDAPI int32_t ADDCALL wav_write(enum wav_proc_type proc_type, void* rx_buf, u_int32_t bytes_to_write);
