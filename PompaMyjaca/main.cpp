#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "pugixml.hpp"
using namespace std;
#define POMPA_MYJACA 8
#define NAZWA_LOGU "logi/PompaMyjaca_log"
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

int sterowanie = 0;
int stanObecny = 0;

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
    int id_kolejki = POMPA_MYJACA;
    key_t msgkey_kolejki;
    int qid_kolejki;
    msgkey_kolejki = (key_t)POMPA_MYJACA;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("./config.xml");
    float pompaMyjacaKrok = doc.child("Zmywarka").child("PompaMyjaca").child("PompaMyjacaKrok").attribute("Wartosc").as_float();

    if(( qid_kolejki = open_queue( msgkey_kolejki)) == -1)
    {
        perror("Otwieranie_kolejki");
        zapiszLog("Problem z otwarciem kolejki");
        exit(1);
    }

    msg.mtype = id_kolejki;
    msg.request = 1;

    while(1)
    {
        read_message(qid_kolejki, id_kolejki, &buf);
        sterowanie = buf.wartosc;
        if (sterowanie != stanObecny)
            stanObecny = sterowanie;
        if (stanObecny)
        {
            printf("Pompa myjaca wlaczona\n");
            zapiszLog("Pompa myjaca wlaczona");
        }
        else
        {
            printf("Pompa myjaca wylaczona\n");
            zapiszLog("Pompa myjaca wylaczona");
        }
        sleep(pompaMyjacaKrok);
    }
}
