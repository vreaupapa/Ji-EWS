#include "ews-common.h"

int main(){
    //generam cheia care e identica cu cea a senzorilor si daemonului
    key_t key = ftok(".", PROJECT_ID);
    if(key < 0){
        perror("Nu s-a putut genera cheia");
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
        printf("[STATUS SISTEM] : ACTIV\n");
        printf("[MESAJE PROCESATE] : %d\n", statistici.total_mesaje);
        printf("[MEDIE SEISMICA CURENTA] : %.2f grade\n", statistici.medie_pericol);

        //daca avem pericol mai mare, afisam pe ecran
        if(statistici.medie_pericol >= THRESHOLD_DANGER) {
            printf("[ALERTA MAXIMA] : EVACUARE/OPRIRE TRENURI !!!\n");
        } else if(statistici.medie_pericol >= 5.0) {
            printf("[ATENTIE] : Activitate seismica moderata.\n");
        } else {
            printf("[STARE] : Normala\n");
        }
        printf("==================================================");
        sleep(1);
    }
    return 0;
}