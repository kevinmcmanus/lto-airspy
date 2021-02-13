/* svshm_string_read.c

    Licensed under GNU General Public License v2 or later.
*/
#include "svshm_string.h"

int
main(int argc, char *argv[])
{
    int semid, shmid;
    //union semun arg, dummy;
    struct sembuf sop;
    char *addr, buf[MEM_SIZE];
    key_t ipc_key;
/*
    if (argc != 3) {
        fprintf(stderr, "Usage: %s shmid semid string\n", argv[0]);
        exit(EXIT_FAILURE);
    }
*/

    /* get the ipc key */
    if ((ipc_key = ftok(IPC_Path, IPC_Proj)) == (key_t) -1) errExit("ftok");

    shmid = shmget(ipc_key, MEM_SIZE, 0600);
    semid = semget(ipc_key, 1, 0600);

    /* Attach shared memory into our address space and copy string
        (including trailing null byte) into memory. */

    addr = shmat(shmid, NULL, 0);
    if (addr == (void *) -1)
        errExit("shmat");


    while (1) {

        /* Wait for semaphore value to become 0 */
        sop.sem_num = 0;
        sop.sem_op = 0;
        sop.sem_flg = 0;

        if (semop(semid, &sop, 1) == -1) {
            if (errno == EIDRM) break;
            errExit("semop1");
        }

        /* get the string */
        strcpy(buf, addr);

        /*release semaphore */
        sop.sem_num = 0;
        sop.sem_op = 2;
        sop.sem_flg = 0;
        if (semop(semid, &sop, 1) == -1)
            errExit("semop2");

        printf("Got string: %s\n", buf);

    } 

    exit(EXIT_SUCCESS);
}