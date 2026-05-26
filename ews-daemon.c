#include "ews-common.h"
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

/*
    -declararea variabilelor globale(threadurile le pot vedea deoarece dau share la aceeasi memorie)
    -declaram mutex pentru a proteja variabilele de mai sus
    pthread_mutex_lock_t lacat_date = PTHREAD_MUTEX_INITIALIZER
    -declararea id ului cozii de mesaje

    -functia pentru fiecare thread in paralel
        -preluare argument si eliberam memoria pt el
        -declaram senzorul si size ul unui mesaj trimis de acesta
        -bucla infinita
            -asteptam sa vedem daca primim mesaj cu mtype 1
            msgrcv(message queue id, &mesaj_senzor, size_mesaj, mtype, 0) - da -1 daca se sterge coada din exterior
            -punem lacatul ca sa nu se calce in picioare threadurile intre ele si sa modifice pe rand
            pthread_mutex_lock(&lacat_date);
            -incrementam numarul de mesaje
            -calculam medie mobila a pericolului
            -scriere pe ecran procesarea unui raport
            -depasire threshold
            -unlock pthread
            pthread_mutex_unlock(&lacat_date);
            //semnal sleep ca sa avem delay intre procesarea lor
        -pthread exit
*/

double nivel_pericol_global = 0.0;
int mesaje_procesate = 0;

pthread_mutex_t lacat_date = PTHREAD_MUTEX_INITIALIZER;

int msgid_global;

void* thread_worker(void* arg){
    int thread_id = *((int*) arg);
    free(arg);

    struct sensor_msg sensor;
    size_t msg_size = sizeof(struct sensor_msg) - sizeof(long);

    while(1){
        if(msgrcv(msgid_global, &sensor, msg_size, 1, 0) == -1){
            printf("S-a sters coada!\n");
            break;
        }

        pthread_mutex_lock(&lacat_date);

        mesaje_procesate++;

        if(nivel_pericol_global == 0.0){
            nivel_pericol_global = sensor.magnitude;
        } else {
            nivel_pericol_global = (nivel_pericol_global + sensor.magnitude) / 2;
        }

        printf("[Thread %d] Procesat raport de la Senzorul %d (Magnitudine: %.1f). Stare globala: %.1f grade.\n",\
             thread_id, sensor.sensor_id, sensor.magnitude, nivel_pericol_global);
            
        fflush(stdout);

        if(nivel_pericol_global > THRESHOLD_DANGER){
            printf("[CRITIC] CUTREMUR MARE DETECTAT (%.1f grade)!\n", nivel_pericol_global);
            //restul codului pentru anuntare device uri
            fflush(stdout);
        }

        pthread_mutex_unlock(&lacat_date);

        sleep(1);
    }
    pthread_exit(NULL);
}

/*
    functie pt cand Daemon ul este oprit

    -mesaj pentru oprire
    -eliberam coada
    msgctl(msgid_global, IPC_RMID, NULL);
    -exit
*/

void handle_shutdown(int sig){
    printf("\n[Daemon] Se opreste sistemul EWS... Stergem resursele din Kernel.\n");

    msgctl(msgid_global, IPC_RMID, NULL);

    exit(EXIT_SUCCESS);
}

/*
    main

    FACEM DAEMON

    -dam fork
    -stergem imediat tatal
    -taiem conexiunea cu terminalul
    setsid() - mai mic decat 0 daca da eroare
    -ignoram semnalul SIGUP(sa nu moara programul cand se inchide terminalul fizic)
    signal(SIGHUP, SIG_IGN);
    -mai facem un fork pentru a de asigura ca nu mai putem redeschide terminalul de control din greseala
    ramane doar al doilea copil
    -inchidem descriptorii standard (STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO)

    -redirectionam output ul intr un fisier de log ca sa vedem totusi ce printeaza Daemon
    -clonam log_fd peste STDOUT si STDERR ca sa vedem absolut totul in log

    CONFIGURAREA COZII SI A SEMNALELOR

    -prindem semnalele de oprire ca sa curatam coada
    -generam cheia unica identica cu cea a senzorilor
    -cream coada de mesaje

    LANSAREA THREAD URILOR

    pthread_t fire_executie[MAX_SENSORS]
    -alocam dinamic pentru fiecare senzor un id si cream firul de executie
    dam sleep intr o bucla infinita pentru a da update o data la un anumit interval de timp
*/

int main(){
    pid_t pid = fork();
    if(pid < 0){
        perror("Fork a esuat");
        exit(EXIT_FAILURE);
    }
    if(pid > 0){
        exit(EXIT_SUCCESS);
    }

    if(setsid() < 0){
        perror("Setsid a esuat");
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if(pid < 0){
        perror("Al doilea Fork a esuat");
        exit(EXIT_FAILURE);
    }
    if(pid > 0){
        exit(EXIT_SUCCESS);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int log_fd = open("ews.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(log_fd != -1){
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
    }

    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);

    key_t key = ftok(".", PROJECT_ID);
    if(key == -1) exit(EXIT_FAILURE);

    msgid_global = msgget(key, 0666 | IPC_CREAT);
    if(msgid_global == -1) exit(EXIT_FAILURE);

    printf("[Daemon EWS] Initializat cu succes ca serviciu de sistem in background.\n");
    fflush(stdout);

    pthread_t fire_executie[MAX_SENSORS];

    for(int i=0; i<MAX_SENSORS; i++){
        int* id_thread = malloc(sizeof(int));
        *id_thread = i + 1;

        if(pthread_create(&fire_executie[i], NULL, thread_worker, id_thread) != 0){
            perror("Eroare la crearea thread ului");
        }
    }

    while(1){
        sleep(10);
    }
}