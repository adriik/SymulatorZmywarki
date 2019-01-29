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

#define STEROWANIE 4
#define GRZALKA 10
#define NAZWA_LOGU "logi/CzujnikTemperatury_log"
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
    if((result = msgrcv( qid, qbuf, length, type,  0)) == -1)
        return(-1);
    return(result);
}


int main()
{

    czyscLog();

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("./config.xml");
    int poczatkowaTemperatura = doc.child("Zmywarka").child("SensorTemperatury").child("PoczatkowaTemperatura").attribute("Wartosc").as_int();
    //int sensorTemperaturyKrok = doc.child("Zmywarka").child("SensorTemperatury").child("SensorTemperaturyKrok").attribute("Wartosc").as_int();
    float dzielnikWartosci = doc.child("Zmywarka").child("Sterowanie").child("DzielnikWartosci").attribute("Wartosc").as_float();

    int cieplo = 0;
    int temperatura = poczatkowaTemperatura;

    // Kolejka CzujnikTemperatury ---> Sterowanie
    int id_kolejkiSterowanie = STEROWANIE;
    key_t msgkey_kolejkiSterowanie;
    int qid_kolejkiSterowanie;
    msgkey_kolejkiSterowanie = (key_t)STEROWANIE;
    if(( qid_kolejkiSterowanie = open_queue( msgkey_kolejkiSterowanie)) == -1)
    {
        perror("Otwieranie_kolejki");
        zapiszLog("Problem z otwarciem kolejki dla Sterowania!");
        exit(1);
    }
    msg.mtype = id_kolejkiSterowanie;
    msg.request = 1;
    // !Kolejka CzujnikTemperatury ---> Sterowanie

    // Kolejka Grzalka ---> CzujnikTemperatury
    int id_kolejkiGrzalka = GRZALKA;
    key_t msgkey_kolejkiGrzalka;
    int qid_kolejkiGrzalka;
    msgkey_kolejkiGrzalka = (key_t)GRZALKA;
    if(( qid_kolejkiGrzalka = open_queue( msgkey_kolejkiGrzalka)) == -1)
    {
        perror("Otwieranie_kolejki");
        zapiszLog("Problem z otwarciem kolejki dla Grzalki!");
        exit(1);
    }

    msg.request = 1;
    // !Kolejka Grzalka ---> CzujnikTemperatury

    while(1) {
        //Odbieranie ciepla od Grzalki
        read_message(qid_kolejkiGrzalka, id_kolejkiGrzalka, &buf);
        cieplo = buf.wartosc;
        temperatura += cieplo;
        if(temperatura < poczatkowaTemperatura)
            temperatura = poczatkowaTemperatura;
        else
        {
            printf("Odebrano cieplo = %.2f\n", (float)cieplo / dzielnikWartosci);
            fflush(stdout);

            snprintf(buforDlaLogu, sizeof(buforDlaLogu), "Odebrano cieplo: %.2f", (float)cieplo / dzielnikWartosci);
            zapiszLog(buforDlaLogu);
        }
        printf("Temperatura = %.2f\n", (float)temperatura / dzielnikWartosci);
        fflush(stdout);

        snprintf(buforDlaLogu, sizeof(buforDlaLogu), "Temperatura: %.2f", (float)temperatura / dzielnikWartosci);
        zapiszLog(buforDlaLogu);

        msg.mtype = id_kolejkiSterowanie;
        msg.wartosc = temperatura;
        if(send_message(qid_kolejkiSterowanie, &msg) == -1)
        {
            perror("Wysylanie do sterowania\n");
            zapiszLog("Problem z wyslaniem temperatury do sterowania!");
            exit(1);
        }
        //sleep(sensorTemperaturyKrok);
    }
}
