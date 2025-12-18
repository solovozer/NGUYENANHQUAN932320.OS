#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    int id;
} Event;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;

Event* event = NULL;
int ready = 0;

/* producer */
void* producer(void* arg) {
    int counter = 0;

    while (1) {
        sleep(1);

        pthread_mutex_lock(&lock);
        if (ready == 1) {
            pthread_mutex_unlock(&lock);
            continue;
        }

        event = (Event*) malloc(sizeof(Event));
        event->id = ++counter;
        ready = 1;

        printf("provided %d\n", event->id);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/* consumer */
void* consumer(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        while (ready == 0) {
            pthread_cond_wait(&cond, &lock);
            printf("awoke\n");
        }

        printf("consumed %d\n", event->id);
        free(event);
        event = NULL;
        ready = 0;

        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main() {
    pthread_t p, c;

    pthread_create(&p, NULL, producer, NULL);
    pthread_create(&c, NULL, consumer, NULL);

    pthread_join(p, NULL);
    pthread_join(c, NULL);
    return 0;
}
