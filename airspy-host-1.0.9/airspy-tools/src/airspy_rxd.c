/* svshm_string_write.c

    Licensed under GNU General Public License v2 or later.
*/
#include "svshm_string.h"
#define MAXITER 20

int
main(int argc, char *argv[])
{
    int semid, shmid, iter;
    struct sembuf sop;
    union semun arg, dummy; //, semval;
    char *addr;
    key_t ipc_key;

    /* create the shared objects */

    /* get the ipc key */
    if ((ipc_key = ftok(IPC_Path, IPC_Proj)) == (key_t) -1) errExit("ftok");

    /* set the semval */
    shmid = shmget(ipc_key, MEM_SIZE, IPC_CREAT | 0600);
    if (shmid == -1)
        errExit("shmget");

    semid = semget(ipc_key, 1, IPC_CREAT | 0600);
    if (shmid == -1)
        errExit("shmget");

    /* Attach shared memory into our address space */

    addr = shmat(shmid, NULL, 0);
    if (addr == (void *) -1)
        errExit("shmat");

    /* Initialize semaphore 0 in set with value 1 */

    arg.val = 2;
    if (semctl(semid, 0, SETVAL, arg) == -1)
        errExit("semctl");

    printf("Cut and Paste the line below:\n");
    printf("./airspy_reader %d  %d\n", shmid, semid);

    /* loop, each iteration waits for semval 0 */
    for (iter=0; iter<MAXITER; iter++){

        /*get the current value
        semval.val = semctl(semid, 0, GETVAL);
        printf("at top of loop: semval: %d, about to wait\n", semval.val);
        */


        /* wait for the semaphore */
        sop.sem_num = 0;
        sop.sem_op = -1;
        sop.sem_flg = 0;
        if (semop(semid, &sop, 1) == -1)
            errExit("semop");

        /*semval.val = semctl(semid, 0, GETVAL);
        printf("at middle of loop: semval: %d, in critical region\n", semval.val);
        */

        sprintf( addr, "Iteration #%d", iter);

        /* release semaphore */
        sop.sem_num = 0;
        sop.sem_op = -1;
        sop.sem_flg = 0;
        if (semop(semid, &sop, 1) == -1)
            errExit("semop");

        /*semval.val = semctl(semid, 0, GETVAL);
        printf("at bottom of loop: semval: %d, released\n", semval.val);
        */

    }

    /* Remove shared memory and semaphore set */

    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        errExit("shmctl");
    if (semctl(semid, 0, IPC_RMID, dummy) == -1)
        errExit("semctl");

    exit(EXIT_SUCCESS);
}