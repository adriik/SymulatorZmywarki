#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "pugixml.hpp"
#include <pthread.h>
#include <stdlib.h>
#include <sys/shm.h>

using namespace std;
#define LICZBA_FLAG 11                  // Liczba flag wspoldzielonych
#define LICZBA_KOLEJEK 9                // Liczba kolejek komunikatow dla sterowania
#define CZUJNIK_PROGRAMU 0              // Indeks flagi - program
#define CZUJNIK_DRZWI 1                 // Indeks flagi - drzwi
#define SENSOR_DOLNEGO_POZIOMU_WODY 2   // Indeks flagi - dolny czujnik wody
#define TEMPERATURY 3                   // Indeks flagi - czujnik temperatury
#define SENSOR_GORNEGO_POZIOMU_WODY 4   // Indeks flagi - gorny czujnik wody
#define POMPA_WLEWAJACA 5               // Indeks flagi - pompa wlewajaca
#define GRZALKA 6                       // Indeks flagi - grzalka
#define POMPA_MYJACA 7                  // Indeks flagi - pompa myjaca
#define POMPA_WYLEWAJACA 8              // Indeks flagi - pompa wylewajaca
#define CYKL 9                          // Indeks flagi - numer cyklu
#define ETAP 10                         // Indeks flagi - etap pracy zmywarki:
                                        // 0 - oczekiwanie na wybor programu
                                        // 1 - oczekiwanie na zamkniecie drzwi
                                        // 2 - nalewanie wody
                                        // 3 - podgrzewanie wody
                                        // 4 - mycie naczyn
                                        // 5 - wylewanie wody
                                        // 6 - oczekiwanie na otworzenie drzwi
                                        // 7 - drzwi zostaly otwarte w trakcie zmywania
#define BLAD 11                         // Indeks flagi - blad programu:
                                        // 0 - brak bledu
                                        // 1 - 0x01 - uszkodzona pompa wlewajaca lub gorny czujnik wody
                                        // 2 - 0x02 - uszkodzona grzalka lub czujnik temperatury
                                        // 3 - 0x03 - uszkodzona pompa wylewajaca lub dolny czujnik wody
                                        // 4 - 0x04 - drzwi zostaly otwarte w trakcie zmywania

#define NAZWA_LOGU "logi/Sterowanie_log"
char buforDlaLogu[255];

// Zmienne globalne przyjmujace wartosci z pliku konfiguracyjnego
float maksymalnaTemperatura;
float minimalnaTemperatura;
float czasOsiagnieciaTemperatury;
float czasNalaniaWody;
float czasWylaniaWody;
float dzielnikWartosci;
float czasMycia;
float pompaMyjacaKrok;
float sensorWodyGornyKrok;
float sensorWodyDolnyKrok;
float sensorTemperaturyKrok;

// Struktura do przechowywania aktualnych czasow wykonywania poszczegolnych etapow
typedef struct Czasy
{
    float czasWlewaniaWody = 0;
    float czasGrzaniaWody = 0;
    float czasMycia = 0;
    float czasWylewaniaWody = 0;
} Czasy;

Czasy czasy;

int *flagi = NULL;
int shmid;
key_t key;
int id_kolejek[] = {1,2,3,4,5,6,7,8,9};
int qid_kolejek[LICZBA_KOLEJEK];

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

// Watki
void *ObslugaWatekPompaMyjaca(void *arg)
{
    Czasy* czasy = (Czasy*)arg;
    while(1)
    {
        // Zwiekszenie czasu mycia naczyn
        //czasy->czasMycia++;
        //sleep(1);
        if(flagi[POMPA_MYJACA] == 1)
        {
            //czasy->czasMycia = 100;
            czasy->czasMycia += pompaMyjacaKrok;
            zapiszLog("Pompa Myjaca: zwiekszenie czasu mycia naczyn");
        }
        if(flagi[ETAP] == 4 && czasy->czasMycia < czasMycia && flagi[POMPA_MYJACA] == 0)
        {
            zapiszLog("Pompa Myjaca: uruchomienie pompy myjacej");
            flagi[POMPA_MYJACA] = 1;
            msg.mtype = POMPA_MYJACA+1;
            msg.request = 1;
            msg.wartosc = 1;
            if((send_message( qid_kolejek[POMPA_MYJACA], &msg )) == -1)
            {
                perror("Wysylanie do pompy myjacej\n");
                zapiszLog("Pompa Myjaca: Problem z wysylaniem do pompy myjacej!");
                exit(1);
            }
        }
        // Wylaczenie pompy myjacej
        else if ((flagi[BLAD] == 4 && flagi[POMPA_MYJACA] == 1) || czasy->czasMycia >= czasMycia && flagi[POMPA_MYJACA] == 1)
        {
            flagi[POMPA_MYJACA] = 0;
            czasy->czasMycia = 0;
            msg.mtype = POMPA_MYJACA+1;
            msg.request = 1;
            msg.wartosc = 0;

            zapiszLog("Pompa Myjaca: wylaczenie pompy myjacej");
            if((send_message( qid_kolejek[POMPA_MYJACA], &msg )) == -1)
            {
                perror("Wysylanie do pompy myjacej\n");
                zapiszLog("Pompa Myjaca: Problem z wysylaniem do pompy myjacej!");
                exit(1);
            }
            // Przejscie do nastepnego etapu
            if (flagi[ETAP] == 4)
            {
                flagi[ETAP] = 5;
                czasy->czasMycia = 0;

                zapiszLog("Pompa Myjaca: przejscie do nastepnego etapu");
            }
        }
        sleep(pompaMyjacaKrok);
    }
}

void *ObslugaWatekPompaWlewajaca(void *arg)
{
    Czasy* czasy = (Czasy*)arg;
    while(1)
    {
        // Zwiekszenie czasu wlewania wody
        if(flagi[POMPA_WLEWAJACA] == 1)
        {
            czasy->czasWlewaniaWody += sensorWodyGornyKrok;
            zapiszLog("Pompa Wlewajaca: zwiekszenie czasu wlewania wody");
        }
        // Sprawdzenie czy zostala uszkodzona pompa wlewajaca lub gorny czujnik wody
        if(czasy->czasWlewaniaWody > czasNalaniaWody)
            flagi[BLAD] = 1;
        read_message(qid_kolejek[SENSOR_GORNEGO_POZIOMU_WODY], id_kolejek[SENSOR_GORNEGO_POZIOMU_WODY], &buf);
        flagi[SENSOR_GORNEGO_POZIOMU_WODY] = buf.wartosc;
        // Wlaczenie pompy wlewajacej
        if(flagi[ETAP] == 2 && flagi[SENSOR_GORNEGO_POZIOMU_WODY] == 0 && flagi[POMPA_WLEWAJACA] == 0)
        {
            flagi[POMPA_WLEWAJACA] = 1;
            msg.mtype = POMPA_WLEWAJACA+1;
            msg.request = 1;
            msg.wartosc = 1;

            zapiszLog("Pompa Wlewajaca: wlaczono pompe wlewajaca");
            if((send_message( qid_kolejek[POMPA_WLEWAJACA], &msg )) == -1)
            {
                perror("Wysylanie do pompy wlewajacej\n");
                zapiszLog("Pompa Wlewajaca: Problem z wysylaniem do pompy wlewajacej!");
                exit(1);
            }
        }
        // Wylaczenie pompy wlewajacej
        else if((flagi[BLAD] == 4 && flagi[POMPA_WLEWAJACA] == 1) || (flagi[BLAD] == 1 && flagi[POMPA_WLEWAJACA] == 1) || ((flagi[SENSOR_GORNEGO_POZIOMU_WODY]) == 1 && flagi[POMPA_WLEWAJACA] == 1))
        {
            flagi[POMPA_WLEWAJACA] = 0;
            msg.mtype = POMPA_WLEWAJACA+1;
            msg.request = 1;
            msg.wartosc = 0;

            zapiszLog("Pompa Wlewajaca: wylaczenie pompy wlewajacej");
            if((send_message( qid_kolejek[POMPA_WLEWAJACA], &msg )) == -1)
            {
                perror("Wysylanie do pompy wlewajacej\n");
                zapiszLog("Pompa Wlewajaca: Problem z wysylaniem do pompy wlewajacej!");
                exit(1);
            }
            // Zatrzymanie zmywarki
            if (flagi[BLAD] == 1)
            {
                zapiszLog("Pompa Wlewajaca: blad 0x01 - uszkodzona pompa wlewajaca lub gorny czujnik wody");
                zapiszLog("Pompa Wlewajaca: zatrzymanie zmywarki");
                flagi[ETAP] = 6;
                czasy->czasWlewaniaWody = 0;
            }
            // Przejscie do nastepnego etapu
            else if (flagi[ETAP] == 2)
            {
                zapiszLog("Pompa Wlewajaca: przejscie do nastepnego etapu");
                flagi[ETAP] = 3;
                czasy->czasWlewaniaWody = 0;
            }
        }
        // Przejscie do nastepnego etapu, gdy w zmywarce jest wymagany poziom wody
        else if (flagi[ETAP] == 2 && flagi[SENSOR_GORNEGO_POZIOMU_WODY] == 1)
        {
            zapiszLog("Pompa Wlewajaca: przejscie do nastepnego etapu, gdy w zmywarce jest wymagany poziom wody");
            if (flagi[CYKL] == 0)
                flagi[CYKL] = 1;
            flagi[ETAP] = 3;
            czasy->czasWlewaniaWody = 0;
        }
    }
}

void *ObslugaWatekPompaWylewajaca(void *arg)
{
    Czasy* czasy = (Czasy*)arg;
    while(1)
    {
        // Zwiekszenie czasu wylewania wody
        if(flagi[POMPA_WYLEWAJACA] == 1)
        {
            czasy->czasWylewaniaWody += sensorWodyDolnyKrok;
            zapiszLog("Pompa Wylewajaca: zwiekszenie czasu wylewania wody");
        }
        // Sprawdzenie czy zostala uszkodzona pompa wylewajaca lub dolny czujnik wody
        if(czasy->czasWylewaniaWody > czasWylaniaWody)
            flagi[BLAD] = 3;
        read_message(qid_kolejek[SENSOR_DOLNEGO_POZIOMU_WODY], id_kolejek[SENSOR_DOLNEGO_POZIOMU_WODY], &buf);
        flagi[SENSOR_DOLNEGO_POZIOMU_WODY] = buf.wartosc;
        // Wlaczenie pompy wylewajacej
        if(flagi[ETAP] == 5 && flagi[SENSOR_DOLNEGO_POZIOMU_WODY] == 1 && flagi[POMPA_WYLEWAJACA] == 0)
        {
            zapiszLog("Pompa Wylewajaca: wlaczenie pompy wylewajacej");
            flagi[POMPA_WYLEWAJACA] = 1;
            msg.mtype = POMPA_WYLEWAJACA+1;
            msg.request = 1;
            msg.wartosc = 1;
            if((send_message( qid_kolejek[POMPA_WYLEWAJACA], &msg )) == -1)
            {
                perror("Wysylanie do pompy wylewajacej\n");
                zapiszLog("Pompa Wylewajaca: Problem z wysylaniem do pompy wylewajacej");
                exit(1);
            }
        }
        // Wylaczenie pompy wylewajacej
        else if((flagi[BLAD] == 4 && flagi[POMPA_WYLEWAJACA] == 1) || (flagi[BLAD] == 3 && flagi[POMPA_WYLEWAJACA] == 1) || (flagi[SENSOR_DOLNEGO_POZIOMU_WODY] == 0 && flagi[POMPA_WYLEWAJACA] == 1))
        {
            flagi[POMPA_WYLEWAJACA] = 0;
            msg.mtype = POMPA_WYLEWAJACA+1;
            msg.request = 1;
            msg.wartosc = 0;

            zapiszLog("Pompa Wylewajaca: wylaczenie pompy wylewajacej");
            if((send_message( qid_kolejek[POMPA_WYLEWAJACA], &msg )) == -1)
            {
                perror("Wysylanie do pompy wylewajacej\n");
                zapiszLog("Pompa Wylewajaca: Problem z wysylaniem do pompy wylewajacej");
                exit(1);
            }
            // Zatrzymanie zmywarki
            if (flagi[BLAD] == 3)
            {
                zapiszLog("Pompa Wylewajaca: blad 0x03 - uszkodzona pompa wylewajaca lub dolny czujnik wody");
                zapiszLog("Pompa Wylewajaca: zatrzymanie zmywarki");
                flagi[ETAP] = 6;
                czasy->czasWylewaniaWody = 0;
            }
            // Zakonczenie mycia
            //if (flagi[BLAD] == 3 || (flagi[ETAP] == 5 && flagi[CYKL] >= flagi[CZUJNIK_PROGRAMU]))
            else if (flagi[ETAP] == 5 && flagi[CYKL] >= flagi[CZUJNIK_PROGRAMU])
            {
                zapiszLog("Pompa Wylewajaca: zakonczenie mycia");
                flagi[ETAP] = 6;
                czasy->czasWylewaniaWody = 0;
            }
            // Kolejny cykl mycia
            else if (flagi[ETAP] == 5 && flagi[CYKL] < flagi[CZUJNIK_PROGRAMU])
            {
                zapiszLog("Pompa Wylewajaca: kolejny cykl mycia");
                flagi[CYKL]++;
                flagi[ETAP] = 2;
                czasy->czasWylewaniaWody = 0;
            }
        }
    }
}

void *ObslugaWatekProgram(void *arg)
{
    while(1)
    {
        buf.request = 1;
        read_message(qid_kolejek[CZUJNIK_PROGRAMU], id_kolejek[CZUJNIK_PROGRAMU], &buf);
        if(flagi[ETAP] == 0 || flagi[ETAP] == 1 || flagi[BLAD] != 0)
        {
            flagi[CZUJNIK_PROGRAMU] = buf.wartosc;
            // Przejscie do nastepnego etapu
            if (flagi[CZUJNIK_DRZWI] == 0)
            {
                flagi[ETAP] = 1;
                zapiszLog("Czujnik Programu: wybrano program");
                //zapiszLog("Czujnik Programu: przejscie do nastepnego etapu");
            }
            else
            {
                flagi[ETAP] = 2;
                flagi[CYKL] = 1;
                zapiszLog("Czujnik Programu: wybrano program");
                zapiszLog("Czujnik Programu: przejscie do nastepnego etapu");
            }
            flagi[BLAD] = 0;
        }
        //sleep(0.1);
    }
}

void *ObslugaWatekDrzwi(void *arg)
{
    while(1)
    {
        buf.request = 1;
        read_message(qid_kolejek[CZUJNIK_DRZWI], id_kolejek[CZUJNIK_DRZWI], &buf);
        if(flagi[CZUJNIK_DRZWI] != buf.wartosc)
        {
            flagi[CZUJNIK_DRZWI] = buf.wartosc;
            // Przejscie do nastepnego etapu, jezeli zostal wybrany program
            if (flagi[ETAP] == 1)
            {
                zapiszLog("Czujnik Drzwi: drzwi zostaly zamkniete");
                zapiszLog("Czujnik Drzwi: przejscie do nastepnego etapu");
                flagi[ETAP] = 2;
                flagi[CYKL] = 1;
            }
            // Zakonczenie cyklu
            else if (flagi[ETAP] == 6 || (flagi[ETAP] == 7 && flagi[CZUJNIK_DRZWI] == 1))
            {
                zapiszLog("Czujnik Drzwi: drzwi zostaly otwarte");
                zapiszLog("Czujnik Drzwi: zakonczenie cyklu");
                flagi[CZUJNIK_PROGRAMU] = 0;
                flagi[ETAP] = 0;
                flagi[CYKL] = 0;
                flagi[BLAD] = 0;
            }
            // Zatrzymanie zmywarki
            else if (flagi[CZUJNIK_DRZWI] == 0 && (flagi[ETAP] == 2 || flagi[ETAP] == 3 || flagi[ETAP] == 4 || flagi[ETAP] == 5))
            {
                zapiszLog("Czujnik Drzwi: blad 0x04 - drzwi zostaly otwarte w trakcie zmywania");
                zapiszLog("Czujnik Drzwi: zatrzymanie zmywarki");
                flagi[BLAD] = 4;
                flagi[ETAP] = 7;
            }
        }
        //sleep(0.1);
    }
}

void *ObslugaWatekGrzalka(void *arg)
{
    Czasy* czasy = (Czasy*)arg;
    while(1)
    {
        if(flagi[GRZALKA] == 1)
        {
            czasy->czasGrzaniaWody += sensorTemperaturyKrok;
            zapiszLog("Grzalka: zwiekszenie czasu grzania wody");
        }
        // Sprawdzenie czy zostala uszkodzona grzalka lub czujnik temperatury
        if(czasy->czasGrzaniaWody > czasOsiagnieciaTemperatury)
            flagi[BLAD] = 2;
        read_message(qid_kolejek[TEMPERATURY], id_kolejek[TEMPERATURY], &buf);
        flagi[TEMPERATURY] = buf.wartosc;
        // Wlaczenie grzalki
        if(flagi[ETAP] == 3 && flagi[TEMPERATURY] < maksymalnaTemperatura && flagi[GRZALKA] == 0)
        {
            flagi[GRZALKA] = 1;
            msg.mtype = GRZALKA+1;
            msg.request = 1;
            msg.wartosc = 1;

            zapiszLog("Grzalka: osiagnieto wymagana temperature");
            zapiszLog("Grzalka: wlaczona");

            if((send_message( qid_kolejek[GRZALKA], &msg )) == -1)
            {
                perror("Wysylanie do grzalki\n");
                zapiszLog("Problem z uruchomieniem grzalki");
                exit(1);
            }
        }
        // Wylaczenie grzalki
        else if((flagi[BLAD] == 4 && flagi[GRZALKA] == 1) || (flagi[BLAD] == 2 && flagi[GRZALKA] == 1) || (flagi[TEMPERATURY] >= maksymalnaTemperatura && flagi[GRZALKA] == 1))
        {
            flagi[GRZALKA] = 0;
            msg.mtype = GRZALKA+1;
            msg.request = 1;
            msg.wartosc = 0;

            zapiszLog("Grzalka: wylaczona");

            if((send_message( qid_kolejek[GRZALKA], &msg )) == -1)
            {
                perror("Wysylanie do grzalki\n");
                zapiszLog("Grzalka: Problem z grzalka");
                exit(1);
            }
            // Zatrzymanie zmywarki
            if (flagi[BLAD] == 2)
            {
                flagi[ETAP] = 6;
                czasy->czasGrzaniaWody = 0;

                zapiszLog("Grzalka: blad 0x02 - uszkodzona grzalka lub czujnik temperatury");
                zapiszLog("Grzalka: zatrzymanie zmywarki");
            }
            // Przejscie do nastepnego etapu
            else if (flagi[ETAP] == 3)
            {
                flagi[ETAP] = 4;
                czasy->czasGrzaniaWody = 0;

                zapiszLog("Grzalka: przejscie do nastepnego etapu");
            }
        }
        // Przejscie do nastepnego etapu, gdy temperatura wody osiagnela wymagana temperature
        else if (flagi[ETAP] == 3 && flagi[TEMPERATURY] >= maksymalnaTemperatura)
        {
            flagi[ETAP] = 4;
            czasy->czasGrzaniaWody = 0;

            zapiszLog("Grzalka: przejscie do nastepnego etapu po osiagnieciu wymaganej temperatury");
        }
        //sleep(1);
    }
}

void resetFlag()
{
	for(int i = 0; i < LICZBA_FLAG; i++)
        flagi[i] = 0;
}

void wyswietlStanZmywarki()
{
    system("clear");
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                   Zmywarka                                   ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    if (flagi[CZUJNIK_PROGRAMU] == 0)
    printf("║                         Program: Oczekiwanie na wybor                        ║\n");
    else
    printf("║                                  Program: %d                                  ║\n", flagi[CZUJNIK_PROGRAMU]);
    printf("╠═══════════════════════════════════════╦══════════════════════════════════════╣\n");
    printf("║               Czujniki                ║               Efektory               ║\n");
    printf("╠═══════════════════════════════════════╬══════════════════════════════════════╣\n");
    if (flagi[CZUJNIK_DRZWI] == 0)
    printf("║ Czujnik drzwi:             Nieaktywny ║");
    else
    printf("║ Czujnik drzwi:                Aktywny ║");
    fflush(stdout);
    if (flagi[POMPA_WLEWAJACA] == 0)
    printf(" Pompa wlewająca:           Wylaczona ║\n");
    else
    printf(" Pompa wlewająca:            Wlaczona ║\n");
    printf("╠═══════════════════════════════════════╬══════════════════════════════════════╣");
    if (flagi[SENSOR_DOLNEGO_POZIOMU_WODY] == 0)
    printf("║ Dolny czujnik wody:        Nieaktywny ║");
    else
    printf("║ Dolny czujnik wody:           Aktywny ║");
    fflush(stdout);
    if (flagi[GRZALKA] == 0)
    printf(" Grzałka:                   Wylaczona ║\n");
    else
    printf(" Grzałka:                    Wlaczona ║\n");
    printf("╠═══════════════════════════════════════╬══════════════════════════════════════╣\n");
    if (flagi[SENSOR_GORNEGO_POZIOMU_WODY] == 0)
    printf("║ Gorny czujnik Wody:        Nieaktywny ║");
    else
    printf("║ Gorny czujnik Wody:           Aktywny ║");
    fflush(stdout);
    if (flagi[POMPA_MYJACA] == 0)
    printf(" Pompa myjaca:              Wylaczona ║\n");
    else
    printf(" Pompa myjąca:               Wlaczona ║\n");
    printf("╠═══════════════════════════════════════╬══════════════════════════════════════╣\n");
    if ((float)flagi[TEMPERATURY] / dzielnikWartosci < 10)
    printf("║ Czujnik Temperatury:             %.2f ║", (float)flagi[TEMPERATURY] / dzielnikWartosci);
    else if ((float)flagi[TEMPERATURY] / dzielnikWartosci < 100)
    printf("║ Czujnik Temperatury:            %.2f ║", (float)flagi[TEMPERATURY] / dzielnikWartosci);
    fflush(stdout);
    if (flagi[POMPA_WYLEWAJACA] == 0)
    printf(" Pompa wylewająca:          Wylaczona ║\n");
    else
    printf(" Pompa wylewająca:           Wlaczona ║\n");
    printf("╠═══════════════════════════════════════╩══════════════════════════════════════╣\n");
    printf("║                                    Cykl: %d                                   ║\n",flagi[CYKL]);
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    if(flagi[BLAD] != 0)
    switch(flagi[BLAD])
    {
    case 1:
    printf("║        Blad: 0x01 - Uszkodzona pompa wlewajaca lub gorny czujnik wody        ║\n");
    break;
    case 2:
    printf("║           Blad: 0x02 - uszkodzona grzalka lub czujnik temperatury            ║\n");
    break;
    case 3:
    printf("║        Blad: 0x03 - uszkodzona pompa wylewajaca lub dolny czujnik wody       ║\n");
    break;
    case 4:
    printf("║            Blad: 0x04 - drzwi zostaly otwarte w trakcie zmywania             ║\n");
    break;
    }
    else
    switch(flagi[ETAP])
    {
    case 0:
    printf("║                     Etap: Oczekiwanie na wybor programu                      ║\n");
    break;
    case 1:
    printf("║                    Etap: Oczekiwanie na zamkniecie drzwi                     ║\n");
    break;
    case 2:
    if (czasy.czasWlewaniaWody < 10)
    printf("║                          Etap: Wlewanie wody (%.2f)                          ║\n",czasy.czasWlewaniaWody);
    else if (czasy.czasWlewaniaWody < 100)
    printf("║                         Etap: Wlewanie wody (%.2f)                          ║\n",czasy.czasWlewaniaWody);
    break;
    case 3:
    if (czasy.czasGrzaniaWody < 10)
    printf("║                        Etap: Podgrzewanie wody (%.2f)                        ║\n",czasy.czasGrzaniaWody);
    else if (czasy.czasGrzaniaWody < 100)
    printf("║                        Etap: Podgrzewanie wody (%.2f)                       ║\n",czasy.czasGrzaniaWody);
    break;
    case 4:
    if (czasy.czasMycia < 10)
    printf("║                          Etap: Mycie naczyn (%.2f)                           ║\n",czasy.czasMycia);
    else if (czasy.czasMycia < 100)
    printf("║                          Etap: Mycie naczyn (%.2f)                          ║\n",czasy.czasMycia);
    break;
    case 5:
    if (czasy.czasWylewaniaWody < 10)
    printf("║                         Etap: Wylewanie wody (%.2f)                          ║\n",czasy.czasWylewaniaWody);
    else if (czasy.czasWylewaniaWody < 100)
    printf("║                         Etap: Wylewanie wody (%.2f)                         ║\n",czasy.czasWylewaniaWody);
    break;
    case 6:
    printf("║                    Etap: Oczekiwanie na otworzenie drzwi                     ║\n");
    break;
    }
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
}

int main(int argc, char *argv[])
{
    czyscLog();

    if((key = ftok(".", 'C')) == -1)
        perror("Blad tworzenia klucza!");

    //Pamiec wspoldzielona dla flag
    shmid = shmget(key, 14*sizeof(int), IPC_CREAT|0600);
	flagi = (int*)shmat(shmid, NULL, 0);

	// Wczytanie parametrow z pliku konfiguracyjnego
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("./config.xml");
    maksymalnaTemperatura = doc.child("Zmywarka").child("Sterowanie").child("MaksymalnaTemperatura").attribute("Wartosc").as_float();
    minimalnaTemperatura = doc.child("Zmywarka").child("Sterowanie").child("MinimalnaTemperatura").attribute("Wartosc").as_float();
    czasOsiagnieciaTemperatury = doc.child("Zmywarka").child("Sterowanie").child("czasOsiagnieciaTemperatury").attribute("Wartosc").as_float();
    czasNalaniaWody = doc.child("Zmywarka").child("Sterowanie").child("czasNalaniaWody").attribute("Wartosc").as_float();
    czasWylaniaWody = doc.child("Zmywarka").child("Sterowanie").child("czasWylaniaWody").attribute("Wartosc").as_float();
    czasMycia = doc.child("Zmywarka").child("Sterowanie").child("CzasMycia").attribute("Wartosc").as_float();
    dzielnikWartosci = doc.child("Zmywarka").child("Sterowanie").child("DzielnikWartosci").attribute("Wartosc").as_float();
    pompaMyjacaKrok = doc.child("Zmywarka").child("PompaMyjaca").child("PompaMyjacaKrok").attribute("Wartosc").as_float();
    sensorWodyGornyKrok = doc.child("Zmywarka").child("SensorGornegoPoziomuWody").child("SensorWodyGornyKrok").attribute("Wartosc").as_float();
    sensorWodyDolnyKrok = doc.child("Zmywarka").child("SensorDolnegoPoziomuWody").child("SensorWodyDolnyKrok").attribute("Wartosc").as_float();
    sensorTemperaturyKrok = doc.child("Zmywarka").child("SensorTemperatury").child("SensorTemperaturyKrok").attribute("Wartosc").as_float();

    maksymalnaTemperatura *= dzielnikWartosci;
    // Utworzenie kolejek
    key_t msgkey_kolejek[LICZBA_KOLEJEK];
    for(int i = 0; i <LICZBA_KOLEJEK; i++)
    {
        //id_kolejek[i] = i + 1;
        msgkey_kolejek[i] = (key_t)id_kolejek[i];
        if((qid_kolejek[i] = open_queue(msgkey_kolejek[i])) == -1)
        {
            perror("Otwieranie kolejki");
            exit(1);
        }
    }

    resetFlag();

    // Utworzenie watkow
    pthread_t watekDrzwi;
    if (pthread_create(&watekDrzwi, NULL, ObslugaWatekDrzwi, NULL))
    {
        printf("błąd przy tworzeniu wątku.");
        abort();
    }

    pthread_t watekProgram;
    if (pthread_create(&watekProgram, NULL, ObslugaWatekProgram, NULL))
    {
        printf("błąd przy tworzeniu wątku.");
        abort();
    }

    pthread_t watekPompaWylewajaca;
    if (pthread_create(&watekPompaWylewajaca, NULL, ObslugaWatekPompaWylewajaca, &czasy))
    {
        printf("błąd przy tworzeniu wątku.");
        abort();
    }

    pthread_t watekPompaWlewajaca;
    if (pthread_create(&watekPompaWlewajaca, NULL, ObslugaWatekPompaWlewajaca, &czasy))
    {
        printf("błąd przy tworzeniu wątku.");
        abort();
    }

    pthread_t watekPompaMyjaca;
    if (pthread_create(&watekPompaMyjaca, NULL, ObslugaWatekPompaMyjaca, &czasy))
    {
        printf("błąd przy tworzeniu wątku.");
        sleep(2);
        abort();
    }

    pthread_t watekGrzalka;
    if (pthread_create(&watekGrzalka, NULL, ObslugaWatekGrzalka, &czasy))
    {
        printf("błąd przy tworzeniu wątku.");
        abort();
    }

    // Program glowny
    while(1)
    {
        wyswietlStanZmywarki();
        sleep(1);
    }
}
