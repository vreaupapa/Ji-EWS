#include "ews-common.h"

volatile sig_atomic_t alerta_critica = 0;

void handle_urgenta(int sig){
    alerta_critica = 1;
}

void handle_exit(int sig){
    remove("dashboard.pid");
    exit(0);
}

int main(){
    //facem un fisier pentru a scrie pid ul procesului dashboard
    FILE *pid_file = fopen("dashboard.pid", "w");
    if(pid_file) {
        fprintf(pid_file, "%d", getpid());
        fclose(pid_file);
    }

    //prindem semnalele alarma si inchidere
    struct sigaction sa_urg, sa_int;
    sa_urg.sa_handler = handle_urgenta;
    sigemptyset(&sa_urg.sa_mask);
    sa_urg.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa_urg, NULL);

    sa_int.sa_handler = handle_exit;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    //generam cheia care e identica cu cea a senzorilor si daemonului
    key_t key = ftok(".", PROJECT_ID);
    if(key < 0){
        perror("Nu s-a putut genera cheia");
        remove("dashboard.pid");
        exit(EXIT_FAILURE);
    }
    //ne conectam la coada existenta
    int msgid = msgget(key, 0666);
    if(msgid < 0){
        perror("Eroare la conectarea la Message Queue sau nu exista");
        exit(EXIT_FAILURE);
    }

    //initializam structura si size ul
    struct stats_msg statistici;
    size_t stats_size = sizeof(struct stats_msg) - sizeof(long);

    while(1){
        //asteptam sa primim un mesaj cu mtype = 2
        if(msgrcv(msgid, &statistici, stats_size, 2, 0) < 0){
            printf("\n[Dashboard] Conexiunea cu Daemonul s a pierdut(coada a fost stearsa)");
            break;
        }
        //curatam terminalul, mereu apare doar un panou de control
        printf("\033[H\033[J");


        //scriere statistici
        printf("   PANOU DE CONTROL - MONITORIZARE LIVE   \n\n");
        printf("==================================================\n");
        if(alerta_critica == 1){
            printf("\033[1;31m");
            printf("\n!!! ALARMA SISTEM !!! EVACUATI IMEDIAT CLADIREA !!!\n\n\n");
            printf("\033[0m");
            printf("==================================================\n");

            alerta_critica = 0;
            sleep(2);
        }
        printf("[STATUS SISTEM] : ACTIV\n");
        printf("[MESAJE PROCESATE] : %d\n", statistici.total_mesaje);
        printf("[MEDIE SEISMICA CURENTA] : %.2f grade\n", statistici.medie_pericol);

        sleep(1);
    }
    remove("dashboard.pid");
    return 0;
}