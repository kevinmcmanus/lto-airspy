
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <linux/limits.h>

#include "svshm_string.h"

#include <airspy.h>
#include <wav.h>

#define AIRSPY_RXMON_VERSION "0.1.0 19 January 2021"
#define AIRSPY_RXMON_DEFAULT_ROOT "Airspy"

#define PATH_FILE_MAX_LEN (FILENAME_MAX)
#define DATE_TIME_MAX_LEN (32)

static void usage(void)
{
	printf("airspy_rxmon v%s\n", AIRSPY_RXMON_VERSION);
	printf("Usage:\n");
    printf("ON|OFF: turns monitoring on or off\n");
	printf("-r <root filename>: root name for received data files\n");
	printf("-d <directory_path>: Place received files into this directory\n");

}

static int mk_fname(char *dirname, char *rootname, char *path_file)
{
    time_t rawtime;
    struct tm * timeinfo;
    char date_time[DATE_TIME_MAX_LEN];

    time (&rawtime);
    timeinfo = localtime (&rawtime);
    /* file format: <dirname>/<rootname>_<date_time>.wav */
    strftime(date_time, DATE_TIME_MAX_LEN, "%Y%m%d_%H%M%S", timeinfo);
    snprintf(path_file, PATH_FILE_MAX_LEN, "%s/%s_%s.wav", dirname, rootname, date_time);

    return 0;
}

int main(int argc, char **argv)
{
    int result, opt, i, nblocks;
    int semid, shmid;
    char *dirname = NULL, *rootname = NULL, *cmd = NULL;
    char dir[PATH_MAX], fname[PATH_MAX], file_path[PATH_MAX], root_name[PATH_MAX];
    key_t ipc_key;
    airspy_ipc_t *as_ipc;
    struct sembuf sop;

    nblocks = 5000; /* default */
    while( (opt = getopt(argc, argv, "-r:d:n:")) != EOF )
	{
		result = AIRSPY_SUCCESS;
        switch (opt)
        {
            case '\1':
                cmd = optarg;
            break;

            case 'd':
                dirname = optarg;
            break;

            case 'r':
                rootname = optarg;
            break;

            case 'n':
                nblocks = atoi(optarg);
            break;

            default:
				printf("unknown argument '-%c %s'\n", opt, optarg);
				usage();
				return EXIT_FAILURE;
        }
    }

    if (!cmd){
        fprintf(stderr, "%s: Must specify ON or OFF\n", argv[0]);
        usage();
        return EXIT_FAILURE;
    }

    if (strcmp(cmd,"ON") != 0 && strcmp(cmd,"OFF") != 0)
    {
        fprintf(stderr,"%s: Invalid Command: %s\n", argv[0], cmd );
        usage();
        return EXIT_FAILURE;
    }

    if (!dirname)
    {
        if (getcwd(dir, PATH_MAX)==0)
        {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }
        dirname=dir;
    }

    if (!rootname)
    {
        strcpy(root_name, AIRSPY_RXMON_DEFAULT_ROOT);
        rootname = root_name;
    }
    
    printf("Command: %s, Directory: %s, Filename root: %s\n",cmd, dirname, rootname);
    printf("Number of blocks: %d\n", nblocks);


    /* set up the ipc */
    /* get the ipc key */
    ipc_key = ftok(IPC_Path, IPC_Proj);
    if (ipc_key == (key_t) -1) errExit("ftok");

    shmid = shmget(ipc_key, MEM_SIZE, 0600);
    if (shmid < 0) errExit("shmget");

    semid = semget(ipc_key, 1, 0600);
    if (semid < 0) errExit("semget");

    /* Attach shared memory into our address spac */
    as_ipc = (airspy_ipc_t*)shmat(shmid, NULL, 0);
    if (as_ipc == (void *) -1) errExit("shmat");
    printf("airspy_rx pid: %d\n", as_ipc->rx_pid);

    /*grab the semaphore */
    sop.sem_num = 0;
    sop.sem_flg = 0;
    sop.sem_op = 1;
    if (semop(semid, &sop, 1) == -1) {
        errExit("semop1");
    }

    if (strcmp(cmd,"ON") == 0)
    {
        as_ipc->nblocks = nblocks;
        strcpy(as_ipc->dirname, dirname);
        strcpy(as_ipc->filename, rootname);

    } else {
        as_ipc->nblocks = 0;

    }

    /*release the semaphore */
    sop.sem_num = 0;
    sop.sem_flg = 0;
    sop.sem_op = -1;
    if (semop(semid, &sop, 1) == -1) {
        errExit("semop1");
    }

    exit(0);
}