
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
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

//spme error handling macros
char err_buf[PATH_FILE_MAX_LEN];
#define errSysError(str) {snprintf(err_buf, PATH_FILE_MAX_LEN, "%s: %s\n", str, strerror(errno));}
#define errReturn(str) { errSysError(str); return(-1);}
#define errAug(str) {strcat( err_buf,str);strcat(err_buf,"\n");}
#define errAugReturn(str) {errAug(str); return (-1);}
#define errPrint {fprintf(stderr,"%s", err_buf);}
#define errFlush {errno=0;err_buf[0]='\0';}

static void usage()
{
	printf("airspy_rxmon v%s\n", AIRSPY_RXMON_VERSION);
	printf("Usage:\n");
    printf("ON|OFF: turns monitoring on or off\n");
	printf("-r <root filename>: root name for received data files\n");
	printf("-d <directory_path>: Place received files into this directory\n");

}

airspy_ipc_t *as_ipc;
int semid, shmid;

int setup_ipc(){
    key_t ipc_key;

    /* get the ipc key */
    ipc_key = ftok(IPC_Path, IPC_Proj);
    if (ipc_key == (key_t) -1) errReturn("ftok");

    shmid = shmget(ipc_key, MEM_SIZE, 0600);
    if (shmid < 0) errReturn("shmget");

    semid = semget(ipc_key, 1, 0600);
    if (semid < 0) errReturn("semget");

    /* Attach shared memory into our address space */
    as_ipc = (airspy_ipc_t*)shmat(shmid, NULL, 0);
    if (as_ipc == (void *) -1) errReturn("shmat");

    return 0;
}

int sem_acquire(){
    /*grab the semaphore */
    struct sembuf sop;
    sop.sem_num = 0;
    sop.sem_flg = 0;
    sop.sem_op = 1;
    if (semop(semid, &sop, 1) == -1) {
        errReturn("semop1");
    }
    return 0;
}
int sem_release(){
    /*decrement the semaphore */
    struct sembuf sop;
    sop.sem_num = 0;
    sop.sem_flg = 0;
    sop.sem_op = -1;
    if (semop(semid, &sop, 1) == -1) {
        errReturn("semop1");
    }
    return 0;
}


int on_command(char *cmd, int argc, char **argv){
    int opt;
    char *dirname = NULL, *rootname = NULL;
    char dir[PATH_MAX], root_name[PATH_MAX];
  
    int nblocks = 5000; /* default */

    printf("***In on_command ***\n");
    while( (opt = getopt(argc, argv, "-r:d:n:")) != EOF )
	{
        switch (opt)
        {

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
				exit(-1);
        }
    }

    if (!dirname)
    {
        if (getcwd(dir, PATH_MAX)==0) errReturn("getcwd");
        dirname=dir;
    }

    if (!rootname)
    {
        strcpy(root_name, AIRSPY_RXMON_DEFAULT_ROOT);
        rootname = root_name;
    }

    if (setup_ipc() < 0) errAugReturn("while executing 'on' command")

    /* bomb out if we're already monitoring */
    if (as_ipc->nblocks > 0) errAugReturn("Monitoring in progress; use airspy_rxmon off to terminate");

    if (nblocks <= 0) errAugReturn("nblocks must be greater than 0");

    if (sem_acquire() <  0) errAugReturn("while executing 'on' command");

    as_ipc->nblocks = nblocks;
    strcpy(as_ipc->dirname, dirname);
    strcpy(as_ipc->filename, rootname);

    if (sem_release() < 0) errAugReturn("while executing 'on' command");
    
    printf("Command: %s, Directory: %s, Filename root: %s\n",cmd, dirname, rootname);
    printf("Number of blocks: %d\n", nblocks);
    printf("airspy_rx pid: %d\n", as_ipc->rx_pid);

    return(0);
}
int status_command(char *cmd, int argc, char **argv){
    char procdir[128];
    struct stat statbuf;
    if (setup_ipc() == 0){
        printf("airspy_rx pid: %d\n", as_ipc->rx_pid);
        sprintf(procdir,"/proc/%d", as_ipc->rx_pid);
        if (stat(procdir, &statbuf) <0){
            errFlush;
            printf("IPC channel exists but no airspy_rx process; need to restart airspy_rx\n");
        } else if (as_ipc->nblocks > 0){
            printf("Currently monitoring\n");    
            printf("Output directory: %s\nOutput filename root: %s\n", as_ipc->dirname, as_ipc->filename);
            printf("Number of blocks per file: %d\n", as_ipc->nblocks);
        } else {
            printf("airspy_rx idling\n");
        }
    } else {
        errFlush;
        printf("No airspy_rx running\n");
    }
    return 0;
}
int off_command(char *cmd, int argc, char **argv){
    if (setup_ipc() <0) errAugReturn("while executing off command");
    sem_acquire();
    as_ipc->nblocks = 0;
    as_ipc->dirname[0] = '\0';
    as_ipc->filename[0] = '\0';
    sem_release();
    return(0);
}

int kill_command(char *cmd, int argc, char **argv){

    if (setup_ipc() <0) errAugReturn("while executing kill command");

    if (kill(as_ipc->rx_pid, SIGINT)<0){
        errSysError("kill");
        errAugReturn("while executing kill command")
    }

    return(0);

}

int help_command(char *cmd, int argc, char **argv){
    usage();
    return 0;
}  

int not_implemented(char *cmd, int argc, char **argv)
{
    int i;
    printf("Command: %s\n", cmd);
    for (i=0; i<argc; i++){
        printf("\tArgument Number: %d, Argument Value: %s\n", i, argv[i]);
    }

    printf("Not implemented, sorry!\n");
    exit(-1);
}

/* Command Processing */

typedef struct{
    char *cmd;
    int (*cmd_func)(char *, int , char **);
} command;

command cmd_tbl[] =
{
    {"start", &not_implemented},
    {"kill", &kill_command},
    {"on", &on_command},
    {"off", &off_command},
    {"status", &status_command},
    {"help", &help_command},
    {NULL, NULL} //table must end with two NULLs
};

int main(int argc, char **argv)
{
    int result, opt;
    command *pcommand;

    if (((opt = getopt(argc, argv, "-")) == EOF) || (opt != '\1')) {
        fprintf(stderr, "command required\n");
        usage();
        exit(-1);
    }

    pcommand = cmd_tbl;
    while (pcommand->cmd){
        if (!strcmp(pcommand->cmd, optarg)){
            if ((result = pcommand->cmd_func(optarg, argc, argv)) < 0) errPrint;
            exit(result);
        }
        pcommand++;
    }
    if (!pcommand->cmd){
        fprintf(stderr, "Invalid command: %s\n", argv[1]);
        usage();
        exit(-1);
    }
    exit(0);

}