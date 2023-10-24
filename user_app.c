#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
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
   

/* Prva nit */
void* thread1 (void *pParam)
{
    srand((unsigned) time(&t));	//initalize rand() 
	while(1)
	{
		/* Provera uslova zavrsetka programa. */
		if (sem_trywait(&semFinishSignal) == 0)
        		break;
    		if(sem_trywait(&startSemaphore1) == 0)
    		{
			pthread_mutex_lock(&mutex);
			/* Brisanje prethodnog sadrzaja niza. */
       			memset(arr, 0, BUF_LEN * sizeof(arr[0]));
			
			/* Generisanje nasumicnih podataka. */
			int k = rand() % 40 + 10 ;
			for(int i = 0; i<k; i++) 
			{
				char ch = 'A' + (rand() % 26);
				arr[i] = ch;
			}
			printf("Prva nit kreirala podatke: %s \n",arr);
    			fflush(stdout);
			/* Upis generisanih podataka u karakterni uredjaj. */
			ioctl(file_desc,1);
			write(file_desc, &arr, BUF_LEN); 
			/* Signaliziranje drugoj niti da je upis podataka zavrsen. */
			pthread_mutex_unlock(&mutex);
			sem_post(&startSemaphore2);
		}
	}
	return NULL;
}

/* Druga nit */
void* thread2 (void *pParam)
{
	while(1)
	{
		/* Provera uslova zavrsetka programa. */
		if (sem_trywait(&semFinishSignal) == 0) 
			break;
	    	if(sem_trywait(&startSemaphore2) == 0)
	    	{
			/* Citanje enkriptovanih podataka iz karakternog uredjaja. */
	    		read(file_desc, arr, BUF_LEN + 1);
			pthread_mutex_lock(&mutex);
	     		printf("Procitani enkriptovani podaci: %s \n", arr);
			printf("CRC vrijednost: %u\n",arr[50]);
		   	fflush(stdout);
			pthread_mutex_unlock(&mutex);
			/* Unos greske. */
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
			/* Upis podataka za dekripciju u karakterni uredjaj. */
			ioctl(file_desc,0);
			write(file_desc, &arr, BUF_LEN + 1);
			/* Signaliziranje trecoj niti da je upis podataka zavrsen. */
			sem_post(&startSemaphore3);
		}
	}
	return NULL;
}

/* Treca nit */
void* thread3 (void *pParam)
{	
	while(1)
	{
		/* Provera uslova zavrsetka programa. */
		if (sem_trywait(&semFinishSignal) == 0)
       			break;
       		if(sem_trywait(&startSemaphore3) == 0)
       		{
			/* Citanje dekriptovanih podataka iz karakternog uredjaja. */
			read(file_desc, arr, BUF_LEN + 1);
			pthread_mutex_lock(&mutex);
			printf("Procitani dekriptovani podaci: %s \n", arr);
    			fflush(stdout);
			pthread_mutex_unlock(&mutex);
   		}
	}
	return NULL;
}

/* Nit kojom se obezbedjuje zavrsetak programa i ubacivanje greske. 
   Program se zavrsava ukoliko je pritisnut taster 'q' ili 'Q'. */
void* get_input(void* param)
{
	while (1)
	{
		if (sem_trywait(&semFinishSignal) == 0)
     			break;
		/* Preuzmanje znaka sa standardnog ulaza. */
		done = getch();
		if(done == 'q' || done == 'Q')
		{			
			/* Obavesti niti da je pritisnut taster za zavrsetak programa. */
		    	sem_post(&semFinishSignal);
	        	sem_post(&semFinishSignal);
	        	sem_post(&semFinishSignal);
			sem_post(&semFinishSignal);				
		}
	}
	return NULL;
}


/* Funkcija vremenske kontrole koje se poziva na svake dvije sekunde. */
void* timer (void *param)
{
	sem_post(&startSemaphore1);
	return 0;
}

int main()
{

	if(BUF_LEN > 50)
	{
		printf("Bafer predugacak, BUF_LEN maksimalne velicine 50");
		return -1;
	}

	/* Otvaranje secure_mailbox fajla. */
	file_desc = open("/dev/secure_mailbox", O_RDWR);
    	if(file_desc < 0) 
   	{
    	    	printf("Cannot open device file...\n");
       	 	return -2;
    	}

	/* Identifikatori niti. */
	pthread_t t1;
	pthread_t t2;
	pthread_t t3;
	pthread_t quit_thread;
	timer_event_t hPrintStateTimer;
	
	/* Formiranje semafora za odgovarajuce niti. */
	sem_init(&startSemaphore1, 0, 0);
	sem_init(&startSemaphore2, 0, 0);
	sem_init(&startSemaphore3, 0, 0);
	sem_init(&semFinishSignal, 0, 0);
	
	
	/* Initialise mutex. */
    	pthread_mutex_init(&mutex, NULL);
    	
	/* Formiranje programskih niti. */
	pthread_create(&t1, NULL, thread1, NULL);
	pthread_create(&t2, NULL, thread2, NULL);
	pthread_create(&t3, NULL, thread3, NULL);
	pthread_create(&quit_thread, NULL, get_input, NULL);
	
	/* Formiranje vremenske kontrole za funkciju timer. */
	timer_event_set(&hPrintStateTimer, 2000, timer, 0, TE_KIND_REPETITIVE);
	
	/* Cekanje na zavrsetak formiranih niti. */
	pthread_join(t3, NULL);
	pthread_join(t2, NULL);
	pthread_join(t1, NULL);
	pthread_join(quit_thread, NULL);
	
	/* Oslobadjanje resursa. */
	sem_destroy(&startSemaphore1);
	sem_destroy(&startSemaphore2);
	sem_destroy(&startSemaphore3);
	sem_destroy(&semFinishSignal);
	pthread_mutex_destroy(&mutex);

	/* Zatvaranje secure_mailbox fajla. */
    	close(file_desc);

    return 0;
}

