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
#define STEROWANIE 9
#define SENSOR 12
#define NAZWA_LOGU "logi/PompaWylewajaca_log"
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
int wartosc;
float sensorWodyDolnyKrok;

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
    wartosc = doc.child("Zmywarka").child("PompaWylewajaca").child("PompaWylewajacaWartosc").attribute("Wartosc").as_int();
    sensorWodyDolnyKrok = doc.child("Zmywarka").child("SensorDolnegoPoziomuWody").child("SensorWodyDolnyKrok").attribute("Wartosc").as_float();

    int id_kolejek[2];
    id_kolejek[0] = STEROWANIE;
    id_kolejek[1] = SENSOR;
    key_t msgkey_kolejek[2];
    int qid_kolejek[2];
    msgkey_kolejek[0] = (key_t)STEROWANIE;
    msgkey_kolejek[1] = (key_t)SENSOR;
    for (int i = 0; i < 2; i++)
    {
        if(( qid_kolejek[i] = open_queue( msgkey_kolejek[i])) == -1)
        {
            perror("Otwieranie_kolejki");
            zapiszLog("Problem z otwarciem kolejki");
            exit(1);
        }
    }

    msg.mtype = SENSOR;
    msg.request = 1;

    while(1)
    {
        read_message(qid_kolejek[0], id_kolejek[0], &buf);
        sterowanie = buf.wartosc;
        if (sterowanie != stanObecny)
        {
            if (sterowanie == 1)
            {
                printf("Pompa wylewajaca zostala wlaczona\n");
                zapiszLog("Pompa wylewajaca zostala wlaczona");
            }
            else
            {
                printf("Pompa wylewajaca zostala wylaczona\n");
                zapiszLog("Pompa wylewajaca zostala wylaczona");
            }
            stanObecny = sterowanie;
        }
        if (sterowanie == 0)
        {
            msg.wartosc = 0;
            if(send_message(qid_kolejek[1], &msg) == -1)
            {
                perror("Wysylanie do sensora dolnego\n");
                zapiszLog("Problem z wysylaniem do sensora dolnego");
                exit(1);
            }
        }
        else
        {
            msg.wartosc = wartosc;
            if(send_message(qid_kolejek[1], &msg) == -1)
            {
                perror("Wysylanie do sensora gornego\n");
                zapiszLog("Problem z wysylaniem do sensora gornego");
                exit(1);
            }
            printf("Wylano %d jednostek wody\n",wartosc);

            snprintf(buforDlaLogu, sizeof(buforDlaLogu), "Wylano %d jednostek wody", wartosc);
            zapiszLog(buforDlaLogu);
        }
        sleep(sensorWodyDolnyKrok);
    }

    return 0;
}
