#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <err.h>
#include <sys/shm.h>
#include "pugixml.hpp"
using namespace std;
#define SHM_SIZE 4
#define ID_ZMIENNEJ_WSP 100
#define WYSYLANIE_INFORMACJI 5
#define ODBIERANIE_INFORMACJI 11
#define NAZWA_LOGU "logi/CzujnikGornegoPoziomuWody_log"
char buforDlaLogu[255];

bool czyscLog()
{
    FILE *plik = NULL;
    plik = fopen(NAZWA_LOGU, "wt");
    if(plik != NULL)
    {
        return true;
    }
    return false;
}

bool zapiszLog(char* tekst)
{
    FILE *plik = NULL;
    plik = fopen(NAZWA_LOGU, "a");
    if(plik != NULL)
    {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(plik, "%02d/%02d/%0d %02d:%02d:%02d: %s\n", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, tekst);
        fclose(plik);
        return true;
    }
    return false;
}

float wysokoscSensora;
float dzielnikWartosci;
//int krok;
int *poziomWody;
bool zmianaPoziomuWody;

struct mymsgbuf {
    long    mtype;          /* typ wiadomości */
    int     request;        /* numer żądania danego działania */
    int     wartosc;
} msg, buf;

int open_queue( key_t keyval ) {
    int     qid;

    if((qid = msgget( keyval, IPC_CREAT | 0660 )) == -1)
        return(-1);
    return(qid);
}

int send_message( int qid, struct mymsgbuf *qbuf ){
    int result, length;

    /* lenght jest rozmiarem struktury minus sizeof(mtype) */
    length = sizeof(struct mymsgbuf) - sizeof(long);

    if((result = msgsnd( qid, qbuf, length, IPC_NOWAIT)) == -1)
          return(-1);
    return(result);
}

int remove_queue( int qid ){
    if( msgctl( qid, IPC_RMID, 0) == -1)
        return(-1);
    return(0);
}

int read_message( int qid, long type, struct mymsgbuf *qbuf ){
    int     result, length;
    /* lenght jest rozmiarem struktury minus sizeof(mtype) */
    length = sizeof(struct mymsgbuf) - sizeof(long);
    if((result = msgrcv( qid, qbuf, length, type,  0)) == -1)
        return(-1);
    return(result);
}

int main(int argc, char *argv[])
{
    czyscLog();

    int shmid;
    key_t key;
    int *shm;
    	/* Tworzymy klucz dla pamieci dzielonej. */
	if ((key = ftok(".", 'A')) == -1)
	{
        errx(1, "Blad tworzenia klucza!");
        zapiszLog("Blad tworzenia klucza!");
    }

    /* Tworzymy segment pamieci dzielonej */
    if ((shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666)) < 0)
    {
    	errx(2, "Blad tworzenia segmentu pamieci dzielonej!");
    	zapiszLog("Blad tworzenia segmentu pamieci dzielonej!");
    }

    /* Dolaczamy pamiec dzielona do naszej przestrzeni danych. */
    if ((shm = (int *)shmat(shmid, NULL, 0)) == (int *) -1)
    {
    	errx(3, "Blad przylaczania pamieci dzielonej!");
    	zapiszLog("Blad przylaczenia pamieci dzielonej!");
    }

	/* Wypelniamy pamiec dzielona danymi. */
    poziomWody=shm;


    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("./config.xml");
    wysokoscSensora = doc.child("Zmywarka").child("SensorGornegoPoziomuWody").child("WysokoscGornegoSensoraWody").attribute("Wartosc").as_float();
    //dzielnikWartosci = doc.child("Zmywarka").child("Sterowanie").child("DzielnikWartosci").attribute("Wartosc").as_float();
    //krok = doc.child("Zmywarka").child("SensorGornegoPoziomuWody").child("SensorWodyGornyKrok").attribute("Wartosc").as_int();

    //printf("wysokoscSensora: %d\n",wysokoscSensora);
    //printf("krok: %d\n",krok);
    *poziomWody=0;
    int id_kolejek[2];
    id_kolejek[0] = ODBIERANIE_INFORMACJI;
    id_kolejek[1] = WYSYLANIE_INFORMACJI;
    key_t msgkey_kolejek[2];
    int qid_kolejek[2];
    msgkey_kolejek[0] = (key_t)ODBIERANIE_INFORMACJI;
    msgkey_kolejek[1] = (key_t)WYSYLANIE_INFORMACJI;
    for (int i = 0; i < 2; i++)
    {
        if(( qid_kolejek[i] = open_queue( msgkey_kolejek[i])) == -1)
        {
            perror("Otwieranie_kolejki");
            zapiszLog("Problem z otwarciem kolejki");
            exit(1);
        }
    }

    msg.mtype = WYSYLANIE_INFORMACJI;
    msg.request = 1;

    while(1)
    {
        read_message(qid_kolejek[0], id_kolejek[0], &buf);
        if (buf.wartosc != 0){
            *poziomWody += buf.wartosc; /// dzielnikWartosci;
            printf("Poziom wody: %d\n",*poziomWody);

            snprintf(buforDlaLogu, sizeof(buforDlaLogu), "Poziom wody: %d", *poziomWody);
            zapiszLog(buforDlaLogu);
        }
        if (*poziomWody >= wysokoscSensora)
        {
            msg.wartosc = 1;
            if(send_message(qid_kolejek[1], &msg) == - 1)
            {
                perror("Wysylanie do sterowania\n");
                zapiszLog("Problem z wysylaniem do Sterowania!");
                exit(1);
            }
            printf("Czujnik aktywny\n");
            zapiszLog("Czujnik aktywny");
        }
        else
        {
            msg.wartosc = 0;
            if(send_message(qid_kolejek[1], &msg) == - 1)
            {
                perror("Wysylanie do sensora gornego\n");
                zapiszLog("Problem z wysylaniem do sensora gornego!");
                exit(1);
            }
            printf("Czujnik nieaktywny\n");
            zapiszLog("Czujnik nieaktywny");
        }
        //sleep(krok);
    }

    shmdt(shm);

	/* Kasujemy segment pamieci dzielonej. */
	shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
