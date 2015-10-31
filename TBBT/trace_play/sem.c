#include <sys/sem.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int errno;

#define SEMPERM 0777
#define TRUE 1
#define FALSE 0

#define MAX_SEM 50
struct {
    int semid;
    char name[256];
} semname[MAX_SEM] =
{
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
/*
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""},
*/
{0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}, {0, ""}
} ;


/* semaphore operations */

int initsem(key_t key, int value)           /* create a semaphore */
{
  int status = 0;
  int semid;

  if ((semid = semget(key, 1, SEMPERM|IPC_CREAT|IPC_EXCL)) == -1) {
       if (errno == EEXIST) {
		printf("semget key %d already exist\n", key);
	    semid = semget(key, 1, 0);
	   }
  } else {
       status = semctl(semid, 0, SETVAL, value);
	}

  if (semid == -1 || status == -1) {
	   printf("%d:\n", getpid());
       perror("initsem() failed");
       return (-1);
  } else
       return semid;
}

//int getsem(key_t key, int setval)           /* create a semaphore */
int getsem(key_t key)           /* create a semaphore */
{
  int status = 0;
  int semid;

  semid = semget(key, 1, 0);

  if (semid == -1 || status == -1) {
	   printf("%d:\n", getpid());
       perror("initsem() failed");
       return (-1);
  } else
       return semid;
}

int waitsem(int semid)           /* wait on a semaphore */
{
  struct sembuf p_buf;
#ifdef NO_SEM
  return;
#endif

  p_buf.sem_num = 0;
  p_buf.sem_op = -1;
  p_buf.sem_flg = SEM_UNDO;

  if (semop(semid, &p_buf, 1) == -1) {
	printf("%d:", getpid());
       perror("waitsem() failed");
       exit(1);
  } else
       return (0);
}

int postsem(int semid)           /* post to a semaphore */
{
  struct sembuf v_buf;

#ifdef NO_SEM
  return;
#endif


  v_buf.sem_num = 0;
  v_buf.sem_op = 1;
  v_buf.sem_flg = SEM_UNDO;

  if (semop(semid, &v_buf, 1) == -1) {
	printf("%d:", getpid());
       perror("postsem() failed");
       exit(1);
  } else
       return (0);
}

void destsem(int semid)          /* destroy a semaphore */
{
  semctl(semid, 0, IPC_RMID, 0);
}

int dest_and_init_sem (key_t key, int value, char * name)
{
    int semid;
    int i;
    semid = getsem (key);
    if (semid!=-1)
        destsem (semid);
    semid = initsem (key, value);
    printf ("%s semid %d for key %d value %d \n", name, semid, key, value);
                                                                                
    if (semid == 0) {
        printf ("semid == 0\n");
        exit(0);
    }
                                                                                
    for (i=0; i<MAX_SEM; i++) {
        if (semname[i].semid == 0) {
            semname[i].semid = semid;
            strcpy(semname[i].name, name);
            break;
        }
    }
                                                                                
    if (i==MAX_SEM) {
        printf ("semname full\n");
        exit (-1);
    }
                                                                                
    return semid;
}


