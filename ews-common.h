#ifndef EWS_COMMON_H
#define EWS_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>

#define MAX_SENSORS 5
#define THRESHOLD_DANGER 4.0 //magnitudinea peste care e nasol
#define PROJECT_ID 'J'
#define TELEGRAM_BOT_TOKEN "8834385893:AAFBw_kZqzW_CcolxR3QZk_ca8W3ZxUTrkw"
#define TELEGRAM_CHAT_ID "5475192103"

//senzor -> daemon (mtype = 1)
struct sensor_msg{
    long mtype;     //tipul mesajului
    //Orice structura pe care vreau sa o bag intr o coada de mesaje trebuie sa aiba
    //pe prima pozitie o variabila de tip long numita mtype 
    //E un sistem de filtrare automat si rapid integrat direct in kernel
    //De exemplu, daca avem mtype = 1 pemntru rapoarte de cutremur si
    //mtype = 2 pentru baterie descarcata, thread-urile din Daemon pot zice:
    //Vreau doar cele care imi raporteaza cutremurul adica mtype = 1
    int sensor_id; //care senzor trimite
    double magnitude;
    int x, y;
};

//daemon -> Dashboard (mtype = 2)

struct stats_msg{
    long mtype;
    double medie_pericol; //se leaga de nivel_pericol_global
    int total_mesaje; //mesajele procesate
    int senzor_cazut;
};

//structutra pentru heartbeat(verificare ca senzorii traiesc), mtype = 3

struct heartbeat_msg{
    long mtype;
    int sensor_id;
};

//structura pentru trenuri, mtype = 4

struct trains_msg{
    long mtype;
    int x, y;
};

#endif