#include "ews-common.h"
#include <time.h>
#include <pthread.h>

int msgid_global;

void* thread_worker(void* arg){
    int senzor_id = *(int *)arg;

    struct heartbeat_msg verif;
    size_t msg_size = sizeof(struct heartbeat_msg) - sizeof(long);

    verif.mtype = 3;
    verif.sensor_id = senzor_id;

    while(1){
        if(msgsnd(msgid_global, &verif, msg_size, 0) == -1){
            perror("Eroare la trimiterea pulsului");
        }
        sleep(5);
    }
}

int main(int argc, char* argv[]){
    //-verificam daca avem numarul de argumente necesar
    if(argc != 2){
        printf("Eroare!\n Format Utilizare: %s <ID_Senzor(int)>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int id_senzor = atoi(argv[1]);
    //-initializam generator de nuemre aleatoare
    srand(time(NULL) ^ (getpid() << 16));


    // -generam cheia pe care o vom folosi pentru a comunica cu message queue(ca sa nu scrie toate deodata)
    // key_t key = ftok(cale, argument) - face un ID unic folosind argumentul si hashing
    key_t key = ftok(".", PROJECT_ID);
    if(key == -1){
        perror("Eroare la generarea cheii cu ftok");
        exit(EXIT_FAILURE);
    }
    //-ne conectam la message queue
    // msgget(key, permisiune | IPC_CREAT) (in caz ca nu a fost creat, folosim IPC_CREAT pentru a il crea, altfel, te conectezi direct la ea)
    msgid_global = msgget(key, 0666 | IPC_CREAT);
    if(msgid_global == -1){
        perror("Eroare la crearea/conectarea la Message Queue");
        exit(EXIT_FAILURE);
    }

    printf("[Senzor %d] Activat si conectat la sistemul EWS.\n", id_senzor);

    pthread_t hrtbt;

    if(pthread_create(&hrtbt, NULL, thread_worker, &id_senzor) != 0){
        perror("Eroare la crearea threadului");
    }

    //-bucla infinita
    while(1){
        //-timp random de dormit deoarece cutremurele nu sunt constante(cred ca o sa pun intre 2 si 7 secunde)
        int sleep_timer = (rand() % 6) + 2;
        sleep(sleep_timer);
        //-generam cutremure random (80% sa fie mic/20% sa fie periculos)
        double magn = 0.0;
        if(rand() % 100 < 80){
            magn = 2.0 + ((double)rand() / RAND_MAX) * 3.0;
        } else {
            magn = 5.0 + ((double)rand() / RAND_MAX) * 3.5;
        }
        //-bagam datele intr un sensor
        struct sensor_msg msg;
        msg.mtype = 1;
        msg.sensor_id = id_senzor;
        msg.magnitude = magn;
        strcpy(msg.location, "Coasta de Est, JP");
        //scapam de size ul mtype ului, el nu face parte din datele brute ale mesajului seismului
        size_t msg_size = sizeof(struct sensor_msg) - sizeof(long);
        //-trimitem datele de la sensor la message queue(neaparat fara marimea lui mtype, acela nu ne trebuie)
        // msgsnd(Message_Queue, &sensor_data, size_data, 0)//0 in caz de succes, -1 in caz de eroare
        if(msgsnd(msgid_global, &msg, msg_size, 0) == -1){
            perror("Senzor: Eroare la msgsnd (nu am putut trimite mesajul)");
        } else {
            printf("[Senzor %d] A detectat activitate seismica: %.1f grade. Raport trimis! \n", id_senzor, magn);
        }
    }
    return 0;
}