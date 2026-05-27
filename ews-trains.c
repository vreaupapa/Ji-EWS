#include "ews-common.h"
#include <fcntl.h>
#include <time.h>
#include <errno.h>

void handle_exit(int sig){
    unlink("trains.pid");
    exit(EXIT_SUCCESS);
}

int main(){
    key_t key = ftok(".", PROJECT_ID);
    if(key == -1){
        perror("Eroare la obtinerea cheii");
        exit(EXIT_FAILURE);
    }

    struct trains_msg train;
    size_t train_size = sizeof(struct trains_msg) - sizeof(long);

    //ne conectam la coada de mesaje
    int msgid = msgget(key, 0666);
    if(msgid == -1){
        perror("Nu exista coada de mesaje!");
        exit(EXIT_FAILURE);
    }

    //handle la semnlul de exit aka CTRL+C
    struct sigaction sa_int;
    sa_int.sa_handler = handle_exit;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    //initializare viteze trenuri Shinkansen si coordonate
    int viteza_t1 = 300, viteza_t2 = 310, viteza_t3 = 295;
    int x_t1, x_t2, x_t3, y_t1, y_t2, y_t3;
    int epicentru_x, epicentru_y;

    srand(time(NULL) ^ (getpid()<<16));

    printf("[SISTEM TRENURI] pornit. Monitorizare coordonate si trafic\n");

    while(1){
        x_t1 = (rand() % 10);
        y_t1 = (rand() % 10);
        x_t2 = (rand() % 10);
        y_t2 = (rand() % 10);
        x_t3 = (rand() % 10);
        y_t3 = (rand() % 10);

        //asteptam mesaj cu mtype = 4
        if(msgrcv(msgid, &train, train_size, 4, IPC_NOWAIT) == -1){
            epicentru_x = train.x;
            epicentru_y = train.y;
        } else if (errno != ENOMSG){
            //in caz ca eroarea e aceea de coada goala, nu inchide procesul, doar daca e orice altceva
            perror("Eroare la msgrcv trenuri");
            break;
        }

        int semnal_cutremur = 0;
        //afisare asemanatoare ca la dashboard
        printf("\033[H\033[J");

        printf("     MONITORIZARE TRAFIC TRENURI DE MARE VITEZA     \n\n\n");
        //verificam distanta fata de epicentru cu o formula (abs(x-x1) + abs(y-y1)) pentru fiecare dupa care setam viteza corespunzator
        if(abs(x_t1-epicentru_x)+abs(y_t1-epicentru_y) <= 2){
            viteza_t1 = 0;
            semnal_cutremur = 1;
        } else {
            viteza_t1 = (rand() % 100) + 250;
        }
        if(abs(x_t2-epicentru_x)+abs(y_t2-epicentru_y) <= 2){
            viteza_t2 = 0;
            semnal_cutremur = 1;
        } else {

            viteza_t2 = (rand() % 100) + 250;
        }
        if(abs(x_t3-epicentru_x)+abs(y_t3-epicentru_y) <= 2){
            viteza_t3 = 0;
            semnal_cutremur = 1;
        } else {

            viteza_t3 = (rand() % 100) + 250;
        }

        if(semnal_cutremur) {
            printf("\033[1;31m");
            printf("!!! RETEA FEROVIARA BLOCATA DE CATRE EWS !!! \n");
            printf("TOATE TRENURILE IN ZONA X:%d Y:%d : FRANARE DE URGENTA (0km/h)\n", epicentru_x, epicentru_y);
            printf("\033[0m");
        } else {
            printf("\033[1;32m"); // Text Verde
            printf("[STATUS RETEA]: SIGURA / EXPLOATARE NORMALA\n");
            printf("\033[0m");
        }

        printf("==============================================\n");
        printf("[TREN 01] Shinkansen Hayabusa [%d, %d] : %d km/h\n", x_t1, y_t1, viteza_t1);
        printf("[TREN 02] Shinkansen Nozomi [%d, %d] : %d km/h\n", x_t2, y_t2, viteza_t2);
        printf("[TREN 03] Shinkansen Komchi [%d, %d] : %d km/h\n", x_t3, y_t3, viteza_t3);
        printf("==============================================\n");

        sleep (2);
    }

    return 0;

}