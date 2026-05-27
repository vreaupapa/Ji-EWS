#include "ews-common.h"
#include <fcntl.h>
#include <time.h>

//Variabila sigatomic pentru a detecta cutremurul
volatile sig_atomic_t cutremur_detectat = 0;

void handle_seism(int sig){
    cutremur_detectat = 1;
}

void handle_exit(int sig){
    unlink("trains.pid");
    exit(EXIT_SUCCESS);
}

int main(){
    //inregistram pid ul trenului folosind sistem calls
    int pid_fd = open("trains.pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(pid_fd != -1){
        char buf[32];
        int len = sprintf(buf, "%d", getpid());
        write(pid_fd, buf, len);
        close(pid_fd);
    }

    struct sigaction sa_seism, sa_int;
    sa_seism.sa_handler = handle_seism;
    sigemptyset(&sa_seism.sa_mask);
    sa_seism.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa_seism, NULL);

    sa_int.sa_handler = handle_exit;
    sigemptyset(&sa_int.sa_mask);
    sa_seism.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    //initializare viteze trenuri Shinkansen
    int viteza_t1 = 300, viteza_t2 = 310, viteza_t3 = 295;

    srand(time(NULL));

    printf("[SISTEM TRENURI] pornit. Astept fluxul de date...\n");

    while(1){
        printf("\033[H\033[J");

        printf("     MONITORIZARE TRAFIC TRENURI DE MARE VITEZA     \n\n\n");

        if(cutremur_detectat) {
            //trenurile sunt notificate
            viteza_t1 = viteza_t2 = viteza_t3 = 0;

            printf("\033[1;31m");
            printf("!!! RETEA FEROVIARA BLOCATA DE CATRE EWS !!! \n");
            printf("TOATE TRENURILE: FRANARE DE URGENTA (0km/h)\n");
            printf("\033[0m");

            cutremur_detectat = 0;
        } else {
            viteza_t1 = (rand() % 100) + 250;
            viteza_t2 = (rand() % 100) + 250;
            viteza_t3 = (rand() % 100) + 250;
            printf("\033[1;32m"); // Text Verde
            printf("[STATUS RETEA]: SIGURA / EXPLOATARE NORMALA\n");
            printf("\033[0m");
        }

        printf("==============================================\n");
        printf("[TREN 01] Shinkansen Hayabusa : %d km/h\n", viteza_t1);
        printf("[TREN 02] Shinkansen Nozomi : %d km/h\n", viteza_t2);
        printf("[TREN 03] Shinkansen Komchi : %d km/h\n", viteza_t3);
        printf("==============================================\n");

        sleep (1);
    }

    unlink("trains.pid");
    return 0;

}