#include "ews-common.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

//declararea variabilelor globale(threadurile le pot vedea deoarece dau share la aceeasi memorie)
double nivel_pericol_global = 0.0;
int mesaje_procesate = 0;
time_t ultima_bataie[MAX_SENSORS];

//declaram mutex pentru a proteja variabilele de mai sus, practic punem un lacat pe date pentru a nu se intercala threadurile intre ele, doar unu acces
//la un moment dat la a schimba valoarea pentru mesaje procesate si nivel_pericol_global
pthread_mutex_t lacat_date = PTHREAD_MUTEX_INITIALIZER;

//declararea id ului cozii de mesaje
int msgid_global;

// functia pentru fiecare thread in paralel
void* thread_worker(void* arg){
    // preluare argument si eliberam memoria pt el
    int thread_id = *((int*) arg);
    free(arg);

    // declaram senzorul si size ul unui mesaj trimis de acesta
    struct sensor_msg sensor;
    size_t msg_size = sizeof(struct sensor_msg) - sizeof(long);

    // bucla infinita
    while(1){
        // asteptam sa vedem daca primim mesaj cu mtype 1
        if(msgrcv(msgid_global, &sensor, msg_size, 1, 0) == -1){
            printf("S-a sters coada!\n");
            break;
        }

        // punem lacatul ca sa nu se calce in picioare threadurile intre ele si sa modifice pe rand
        pthread_mutex_lock(&lacat_date);

        // incrementam numarul de mesaje
        mesaje_procesate++;

        //calculam medie mobila a pericolului
        if(nivel_pericol_global == 0.0){
            nivel_pericol_global = sensor.magnitude;
        } else {
            nivel_pericol_global = (nivel_pericol_global + sensor.magnitude) / 2.0;
        }

        // scriere pe ecran procesarea unui raport
        printf("[Thread %d] Procesat raport de la Senzorul %d (Magnitudine: %.1f). Stare globala: %.1f grade.\n",\
             thread_id, sensor.sensor_id, sensor.magnitude, nivel_pericol_global);
        //toate flushurile sunt folosite pentru a afisa imediat in ews.log ce mesaj am obtinut, altfel ele sunt puse doar in blocuri mari de 
        //2048 bytes sau ceva de genu
        fflush(stdout);
        // depasire threshold
        if(nivel_pericol_global > THRESHOLD_DANGER){
            printf("[CRITIC] CUTREMUR MARE DETECTAT (%.1f grade)!\n", nivel_pericol_global);
            fflush(stdout);

            FILE* pid_file = fopen("dashboard.pid", "r");
            if(pid_file != NULL){
                pid_t dash_pid;

                if(fscanf(pid_file, "%d", &dash_pid) == 1){
                    kill(dash_pid, SIGUSR1);
                    printf("[DAEMON] Semnal SIGUSR1 trimis catre Dashboard (PID: %d)\n", dash_pid);
                    fflush(stdout);
                }
                fclose(pid_file);
            }
        }
        //declarare + populare structura pentru dashboard
        struct stats_msg pachet_statistici;
        pachet_statistici.mtype = 2;
        pachet_statistici.medie_pericol = nivel_pericol_global;
        pachet_statistici.total_mesaje = mesaje_procesate;
        pachet_statistici.senzor_cazut = 0;

        size_t stats_size = sizeof(struct stats_msg) - sizeof(long);
        //trimitem mesaj in message_queue, deoarece avem mtype = 2, programul stie automat cum sa le selecteze, care pentru ce proces
        msgsnd(msgid_global, &pachet_statistici, stats_size, IPC_NOWAIT);
        //unlock pthread - am terminat de scris ceea ce aveam asa ca dam unlock pentru ca urmatorul thread sa inceapa sa isi faca treaba
        pthread_mutex_unlock(&lacat_date);
        //punem la somn ca in caz de trafic intens de date, sa nu ia si sa sccrie incontinuu un singur thread iar celelalte sa stea degeaba
        sleep(1);
    }
    pthread_exit(NULL);
}

//functie pentru thread ul pulsului
void* thread_puls(void* arg){
    struct heartbeat_msg hrtbt;
    size_t msg_size = sizeof(struct heartbeat_msg) - sizeof(long);

    while(1){
        if(msgrcv(msgid_global, &hrtbt, msg_size, 3, 0) == -1){
            perror("Eroare, s a inchis coada de asteptare");
            break;
        }

        pthread_mutex_lock(&lacat_date);

        printf("[DAEMON] Puls receptionat de la senzorul cu ID %d\n", hrtbt.sensor_id);
        fflush(stdout);

        ultima_bataie[hrtbt.sensor_id - 1] = time(NULL);

        pthread_mutex_unlock(&lacat_date);

        sleep(1);
    }
    pthread_exit(NULL);
}

//functie pentru cand daemonul va fi oprit
void handle_shutdown(int sig){
    printf("\n[Daemon] Se opreste sistemul EWS... Stergem resursele din Kernel.\n");
    //eliberare coada cu IPC_RMID
    msgctl(msgid_global, IPC_RMID, NULL);

    exit(EXIT_SUCCESS);
}


int main(){
    //procesul clasic pentru a crea un daemon
    //dam fork, stergem tatal, punem programul sa ignore semnalul de inchidere al termnalului copii prin signal(SIGHUP, SIG_IGN)
    //astfel daca terminalul se inchide, procesul nostru supravietuieste
    // dam setsid pentru a se separa de terminal , dupa care un alt fork pentru a nu fi lider de sesiune si sa poata deschida singur
    //un terminal de control, ceea ce face setsid, fiind copil fara tata este complet in intuneric, poate fi inchis doar cu comenzi specifice
    //cum ar fi kill
    pid_t pid = fork();
    if(pid < 0){
        perror("Fork a esuat");
        exit(EXIT_FAILURE);
    }
    if(pid > 0){
        exit(EXIT_SUCCESS);
    }

    signal(SIGHUP, SIG_IGN);

    if(setsid() < 0){
        perror("Setsid a esuat");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if(pid < 0){
        perror("Al doilea Fork a esuat");
        exit(EXIT_FAILURE);
    }
    if(pid > 0){
        exit(EXIT_SUCCESS);
    }

    //setam vectorul timp cu timpul la care s acreat deamon ul
    for(int i=0; i<MAX_SENSORS; i++){
        ultima_bataie[i] = time(NULL);
    }

    //inchidem descriptorii standard (STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO)
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    //redirectionam output ul intr un fisier de log ca sa vedem totusi ce printeaza Daemon
    //folosind dup2 care da duplicate la log_fd, tot ce ar fi scris in stdout si stderr ajunge in log_fd
    int log_fd = open("ews.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(log_fd != -1){
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
    }

    //declarare sigaction
    struct sigaction sa;
    sa.sa_handler = handle_shutdown;
    sigemptyset(&sa.sa_mask);
    //NE ASIGURAM CA MSGRCV() NU CRAPA CU EROARE DACA PRIMESTE UN SEMNAL IN TIMP CE DOARME
    sa.sa_flags = SA_RESTART;

    //prindem semnalele de oprire ca sa curatam coada
    if(sigaction(SIGINT, &sa, NULL) < 0){
        perror("Eroare sigaction SIGINT");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGTERM, &sa, NULL) < 0){
        perror("Eroare sigaction SIGTERM");
        exit(EXIT_FAILURE);
    }

    //CONFIGURAREA COZII SI A SEMNALELOR
    //generam cheia unica identica cu cea a senzorilor
    key_t key = ftok(".", PROJECT_ID);
    if(key == -1) exit(EXIT_FAILURE);

    //cream coada de mesaje sau ne conectam la ea
    msgid_global = msgget(key, 0666 | IPC_CREAT);
    if(msgid_global == -1) exit(EXIT_FAILURE);

    printf("[Daemon EWS] Initializat cu succes ca serviciu de sistem in background.\n");
    fflush(stdout);

    //LANSAREA THREAD URILOR
    pthread_t fire_executie[MAX_SENSORS];

    for(int i=0; i<MAX_SENSORS; i++){
        //alocam dinamic pentru fiecare senzor un id si cream firul de executie
        int* id_thread = malloc(sizeof(int));
        *id_thread = i + 1;

        if(pthread_create(&fire_executie[i], NULL, thread_worker, id_thread) != 0){
            perror("Eroare la crearea thread ului");
        }
    }

    pthread_t thread_colector_puls;
    if(pthread_create(&thread_colector_puls, NULL, thread_puls, NULL) != 0){
        perror("Eroare la crearea thread ului colector");
    }

    while(1){
        for(int i=0; i<MAX_SENSORS; i++){
            pthread_mutex_lock(&lacat_date);
            if(time(NULL) - ultima_bataie[i] > 15){
                ultima_bataie[i] = time(NULL);
                printf("[WATCHDOG] Eroare CRITICA! senzorul %d a picat\n", i+1);
                fflush(stdout);
                struct stats_msg picat;
                size_t msg_size = sizeof(struct stats_msg) - sizeof(long);

                picat.mtype = 2;
                picat.medie_pericol = 0.0;
                picat.total_mesaje = 0;
                picat.senzor_cazut = 1;

                if(msgsnd(msgid_global, &picat, msg_size, IPC_NOWAIT) == -1){
                    perror("Eroare trimitere mesaj");
                }
            }
            pthread_mutex_unlock(&lacat_date);
        }
        sleep(2);
    }
}