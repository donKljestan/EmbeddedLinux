#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "timer_event.h"

#define BUF_LEN 50

int file_desc;
volatile char done = 0x0;
static unsigned char arr[BUF_LEN + 1];
char getch(void);

static sem_t startSemaphore1;
static sem_t startSemaphore2;
static sem_t startSemaphore3;
static sem_t semFinishSignal;

static pthread_mutex_t mutex;

time_t t;

/* Blocking wait; SIGALRM from the timer interrupts sem_wait (EINTR), so retry. */
static int wait_sem(sem_t *s)
{
	while (sem_wait(s) != 0)
	{
		if (errno != EINTR)
			return -1;
	}
	return 0;
}

static int quitting(void)
{
	return done == 'q' || done == 'Q';
}
   

/* First thread */
void* thread1 (void *pParam)
{
    srand((unsigned) time(&t));	//initalize rand() 
	while(1)
	{
		/* Check the program termination condition. */
		if (sem_trywait(&semFinishSignal) == 0)
        		break;
    		if(wait_sem(&startSemaphore1) == 0 && !quitting())
    		{
			pthread_mutex_lock(&mutex);
			/* Clear the previous array contents. */
       			memset(arr, 0, BUF_LEN * sizeof(arr[0]));
			
			/* Generate random data. */
			int k = rand() % 40 + 10 ;
			for(int i = 0; i<k; i++) 
			{
				char ch = 'A' + (rand() % 26);
				arr[i] = ch;
			}
			printf("First thread created data: %.*s \n", BUF_LEN, arr);
    			fflush(stdout);
			/* Write the generated data to the character device. */
			ioctl(file_desc,1);
			write(file_desc, &arr, BUF_LEN); 
			/* Signal the second thread that the write is done. */
			pthread_mutex_unlock(&mutex);
			sem_post(&startSemaphore2);
		}
	}
	return NULL;
}

/* Second thread */
void* thread2 (void *pParam)
{
	while(1)
	{
		/* Check the program termination condition. */
		if (sem_trywait(&semFinishSignal) == 0) 
			break;
	    	if(wait_sem(&startSemaphore2) == 0 && !quitting())
	    	{
			/* Read the encrypted data from the character device. */
	    		read(file_desc, arr, BUF_LEN + 1);
			pthread_mutex_lock(&mutex);
	     		printf("Read encrypted data: %.*s \n", BUF_LEN, arr);
			printf("CRC value: %u\n",arr[50]);
		   	fflush(stdout);
			pthread_mutex_unlock(&mutex);
			/* Error injection. */
	       		if(done == 'x' || done == 'X')
	       		{
	       			arr[0] = ~arr[0];
	       			int temp; 
	       			for(int i=0;15>i;i++) 
	       			{ 
	       				temp=rand()%49; 
	       				arr[temp]= ~arr[temp]; 
	       			}
	       		}
			/* Write the data to the character device for decryption. */
			ioctl(file_desc,0);
			write(file_desc, &arr, BUF_LEN + 1);
			/* Signal the third thread that the write is done. */
			sem_post(&startSemaphore3);
		}
	}
	return NULL;
}

/* Third thread */
void* thread3 (void *pParam)
{	
	while(1)
	{
		/* Check the program termination condition. */
		if (sem_trywait(&semFinishSignal) == 0)
       			break;
       		if(wait_sem(&startSemaphore3) == 0 && !quitting())
       		{
			/* Read the decrypted data from the character device. */
			read(file_desc, arr, BUF_LEN + 1);
			pthread_mutex_lock(&mutex);
			printf("Read decrypted data: %.*s \n", BUF_LEN, arr);
    			fflush(stdout);
			pthread_mutex_unlock(&mutex);
   		}
	}
	return NULL;
}

/* Thread that handles program termination and error injection. 
   The program ends when the 'q' or 'Q' key is pressed. */
void* get_input(void* param)
{
	while (1)
	{
		if (sem_trywait(&semFinishSignal) == 0)
     			break;
		/* Read a character from standard input. */
		done = getch();
		if(done == 'q' || done == 'Q')
		{			
			/* Notify the threads that the quit key was pressed. */
		    	sem_post(&semFinishSignal);
	        	sem_post(&semFinishSignal);
	        	sem_post(&semFinishSignal);
			sem_post(&semFinishSignal);				
			/* Wake the threads blocked on their semaphores; must come after the finish signals. */
			sem_post(&startSemaphore1);
			sem_post(&startSemaphore2);
			sem_post(&startSemaphore3);
		}
	}
	return NULL;
}


/* Timer function, called every two seconds. */
void* timer (void *param)
{
	sem_post(&startSemaphore1);
	return 0;
}

int main()
{

	if(BUF_LEN > 50)
	{
		printf("Buffer too long, BUF_LEN has a maximum size of 50\n");
		return -1;
	}

	/* Open the secure_mailbox device file. */
	file_desc = open("/dev/secure_mailbox", O_RDWR);
    	if(file_desc < 0) 
   	{
    	    	printf("Cannot open device file...\n");
       	 	return -2;
    	}

	/* Thread identifiers. */
	pthread_t t1;
	pthread_t t2;
	pthread_t t3;
	pthread_t quit_thread;
	timer_event_t hPrintStateTimer;
	
	/* Create the semaphores for the threads. */
	sem_init(&startSemaphore1, 0, 0);
	sem_init(&startSemaphore2, 0, 0);
	sem_init(&startSemaphore3, 0, 0);
	sem_init(&semFinishSignal, 0, 0);
	
	
	/* Initialise mutex. */
    	pthread_mutex_init(&mutex, NULL);
    	
	/* Create the program threads. */
	pthread_create(&t1, NULL, thread1, NULL);
	pthread_create(&t2, NULL, thread2, NULL);
	pthread_create(&t3, NULL, thread3, NULL);
	pthread_create(&quit_thread, NULL, get_input, NULL);
	
	/* Set up the timer for the timer function. */
	timer_event_set(&hPrintStateTimer, 2000, timer, 0, TE_KIND_REPETITIVE);
	
	/* Wait for the threads to finish. */
	pthread_join(t3, NULL);
	pthread_join(t2, NULL);
	pthread_join(t1, NULL);
	pthread_join(quit_thread, NULL);
	
	/* Release resources. */
	sem_destroy(&startSemaphore1);
	sem_destroy(&startSemaphore2);
	sem_destroy(&startSemaphore3);
	sem_destroy(&semFinishSignal);
	pthread_mutex_destroy(&mutex);

	/* Close the secure_mailbox device file. */
    	close(file_desc);

    return 0;
}

