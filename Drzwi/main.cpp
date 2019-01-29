#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "pugixml.hpp"
#define CZUJNIK_DRZWI 2
using namespace std;
#define NAZWA_LOGU "logi/Drzwi_log"
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

int drzwi;
int stanObecny;

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
    if((result = msgrcv( qid, qbuf, length, type,  IPC_NOWAIT)) == -1)
        return(-1);
    return(result);
}

int main(int argc, char *argv[])
{
    czyscLog();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("./config.xml");
    drzwi = doc.child("Zmywarka").child("CzujnikDrzwi").child("DrzwiStanPoczatkowy").attribute("Wartosc").as_int();
    stanObecny = doc.child("Zmywarka").child("CzujnikDrzwi").child("DrzwiStanPoczatkowy").attribute("Wartosc").as_int();
    int czujnikDrzwiKrok = doc.child("Zmywarka").child("CzujnikDrzwi").child("CzujnikDrzwiKrok").attribute("Wartosc").as_int();

    int id_kolejki = CZUJNIK_DRZWI;
    int qid;
    key_t msgkey;
    msgkey = (key_t)id_kolejki;
    if(( qid = open_queue( msgkey)) == -1)
	{
        perror("Otwieranie_kolejki");
        zapiszLog("Problem z otwarciem kolejki!");
        exit(1);
    }
    msg.mtype = id_kolejki;
    msg.request = 1;

    msg.wartosc = drzwi;
    if((send_message( qid, &msg )) == -1)
    {
        perror("Wysylanie\n");
        zapiszLog("Problem z wysylaniem!");
        exit(1);
    }

    while(1)
    {
        printf("[0] Otworzenie drzwi\n");
        printf("[1] Zamkniecie drzwi\n");
        scanf("%d",&drzwi);
        if (drzwi == 0 || drzwi == 1)
        {
            if (drzwi != stanObecny)
            {
                stanObecny = drzwi;
                msg.wartosc = drzwi;
                if((send_message( qid, &msg )) == -1)
                {
                    perror("Wysylanie\n");
                    zapiszLog("Problem  z wysylaniem");
                    exit(1);
                }
                if (drzwi == 0)
                {
                    printf("Drzwi zostaly otworzone!\n");
                    zapiszLog("Drzwi zostaly otworzone!");
                }
                else
                {
                    printf("Drzwi zostaly zamkniete!\n");
                    zapiszLog("Drzwi zostaly zamkniete!");
                }
            }
            else if (drzwi == 0)
            {
                printf("Drzwi sa juz otwarte!\n");
                //zapiszLog("Drzwi sa juz otwarte!");
            }
            else
            {
                printf("Drzwi sa juz zamkniete!\n");
                //zapiszLog("Drzwi sa juz zamkniete!");
            }
        }
        else
        {
            printf("Wybrano zly kod operacji\n");
            //zapiszLog("Wybrano zly kod operacji");
        }
        printf("\n");
        usleep(czujnikDrzwiKrok);
    }
    return 0;
}
