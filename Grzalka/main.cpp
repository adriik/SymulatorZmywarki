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

#define GRZALKA 7
#define TEMPERATURY 10
#define NAZWA_LOGU "logi/Grzalka_log"
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
    if((result = msgrcv( qid, qbuf, length, type, IPC_NOWAIT)) == -1)
        return(-1);
    return(result);
}


int main()
{
    czyscLog();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("./config.xml");
    float grzalkaWartosc = doc.child("Zmywarka").child("Grzalka").child("GrzalkaWartosc").attribute("Wartosc").as_float();
    float grzalkaWartoscUjemna = doc.child("Zmywarka").child("Grzalka").child("GrzalkaWartoscUjemna").attribute("Wartosc").as_float();
    float sensorTemperaturyKrok = doc.child("Zmywarka").child("SensorTemperatury").child("SensorTemperaturyKrok").attribute("Wartosc").as_float();

    int sterowanie = 0;
    // Kolejka Sterowanie ---> Grzalka
    int id_kolejkiSterowanie = GRZALKA;//GRZALKAAA;
    key_t msgkey_kolejkiSterowanie;
    int qid_kolejkiSterowanie;
    msgkey_kolejkiSterowanie = (key_t)GRZALKA;//GRZALKAAA;
    if(( qid_kolejkiSterowanie = open_queue( msgkey_kolejkiSterowanie)) == -1)
    {
        perror("Otwieranie_kolejki");
        zapiszLog("Problem z otwarciem kolejki dla Sterowania!");
        exit(1);
    }

    int id_kolejkiCzujnikTemperatury = TEMPERATURY;
    key_t msgkey_kolejkiCzujnikTemperatury;
    int qid_kolejkiCzujnikTemperatury;
    msgkey_kolejkiCzujnikTemperatury = (key_t)TEMPERATURY;
    if(( qid_kolejkiCzujnikTemperatury = open_queue( msgkey_kolejkiCzujnikTemperatury)) == -1)
    {
        perror("Otwieranie_kolejki");
        zapiszLog("Problem z otwarciem kolejki dla Czujnika Temperatury!");
        exit(1);
    }
    msg.mtype = 1;
    msg.request = 1;

    while(1) {
        // Wlaczenie grzalki
        read_message(qid_kolejkiSterowanie, id_kolejkiSterowanie, &buf);
        sterowanie = buf.wartosc;
        if(sterowanie == 1) { //Wlaczenie grzalki i wysylanie ciepla do czujnika
            printf("Grzalka wlaczona!\n");
            fflush(stdout);

            zapiszLog("Grzalka wlaczona!");

            msg.mtype = TEMPERATURY;
            msg.request = 1;
            msg.wartosc = grzalkaWartosc;
            if(send_message(qid_kolejkiCzujnikTemperatury, &msg) == -1)
            {
                perror("Wysylanie do czujnika temperatury\n");
                zapiszLog("Problem z wyslaniem ciepla do Czujnika Temperatury!");
                exit(1);
            }
        }
        // Wylaczenie grzalki
        else
        {
            msg.mtype = TEMPERATURY;
            msg.request = 1;
            msg.wartosc = grzalkaWartoscUjemna;
            if(send_message(qid_kolejkiCzujnikTemperatury, &msg) == -1)
            {
                perror("Wysylanie do czujnika temperatury\n");
                zapiszLog("Problem z wyslaniem ciepla do Czujnika Temperatury!");
                exit(1);
            }
            printf("Grzalka wylaczona!\n");

            zapiszLog("Grzalka wylaczona!");
        }
        sleep(sensorTemperaturyKrok);
    }
}
