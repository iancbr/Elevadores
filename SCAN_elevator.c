#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_REQUESTS 3
#define MAX_ELEVATORS 2
#define MAX_FLOORS 10
#define MAX_PASSENGERS 4

typedef struct {
  int to_floor;
  int passenger_id;
} Request;

Request floor_buffers[MAX_FLOORS][MAX_REQUESTS];
int request_counts[MAX_FLOORS] = {0};
sem_t floor_semaphores[MAX_FLOORS];
pthread_mutex_t floor_mutexes[MAX_FLOORS];
int passenger_counter = 0;
int total_requests = 0;
int active_requests = 1; // Indicador global de requisições ativas

void *elevator(void *arg) {
  int elevator_id = *((int *)arg);
  int current_floor = 0;
  int direction = 1; // 1 = subindo, -1 = descendo
  Request passengers[MAX_PASSENGERS];
  int passenger_count = 0;

  while (1) {
    int no_requests = 1; // Indicador de ausência de requisições

    // Mostrar posição atual
    printf("Elevador %d: No andar %d\n", elevator_id, current_floor);

    // Desembarcar passageiros no andar atual
    for (int i = 0; i < passenger_count; i++) {
      if (passengers[i].to_floor == current_floor) {
        printf("Elevador %d: Passageiro %d desembarcou no andar %d\n",
               elevator_id, passengers[i].passenger_id, current_floor);
        for (int j = i; j < passenger_count - 1; j++) {
          passengers[j] = passengers[j + 1];
        }
        passenger_count--;
        i--;
        sleep(1); // Tempo para desembarque
      }
    }

    // Embarcar passageiros no andar atual
    pthread_mutex_lock(&floor_mutexes[current_floor]);
    for (int i = 0; i < request_counts[current_floor]; i++) {
      if (passenger_count < MAX_PASSENGERS &&
          ((direction == 1 &&
            floor_buffers[current_floor][i].to_floor > current_floor) ||
           (direction == -1 &&
            floor_buffers[current_floor][i].to_floor < current_floor))) {
        passengers[passenger_count++] = floor_buffers[current_floor][i];
        printf("Elevador %d: Passageiro %d entrou no elevador no andar %d com "
               "destino ao andar %d\n",
               elevator_id, floor_buffers[current_floor][i].passenger_id,
               current_floor, floor_buffers[current_floor][i].to_floor);

        for (int j = i; j < request_counts[current_floor] - 1; j++) {
          floor_buffers[current_floor][j] = floor_buffers[current_floor][j + 1];
        }
        request_counts[current_floor]--;
        i--;
        no_requests = 0; // Ainda existem requisições ativas
        sleep(1);
      }
    }
    pthread_mutex_unlock(&floor_mutexes[current_floor]);

    // Verificar se ainda há requisições
    for (int i = 0; i < MAX_FLOORS; i++) {
      pthread_mutex_lock(&floor_mutexes[i]);
      if (request_counts[i] > 0) {
        no_requests = 0;
      }
      pthread_mutex_unlock(&floor_mutexes[i]);
    }

    // Se não há requisições, exibir mensagem e sair
    if (no_requests && passenger_count == 0) {
      printf("Elevador %d: Não há mais requisições para atender.\n",
             elevator_id);
      break;
    }

    // Mudar direção se necessário
    int has_requests_in_direction = 0;
    for (int i = current_floor + direction; i >= 0 && i < MAX_FLOORS;
         i += direction) {
      if (request_counts[i] > 0 ||
          (passenger_count > 0 && ((direction == 1 && i > current_floor) ||
                                   (direction == -1 && i < current_floor)))) {
        has_requests_in_direction = 1;
        break;
      }
    }

    if (!has_requests_in_direction) {
      direction = -direction; // Inverter direção
      printf("Elevador %d: Mudando direção para %s\n", elevator_id,
             direction == 1 ? "subindo" : "descendo");
    } else {
      current_floor += direction;
    }

    sleep(1);
  }

  return NULL;
}

void make_request() {
  if (total_requests >= MAX_REQUESTS) {
    return;
  }

  int from = rand() % MAX_FLOORS;
  int to = rand() % MAX_FLOORS;
  while (to == from) {
    to = rand() % MAX_FLOORS;
  }

  Request req = {to, ++passenger_counter};
  pthread_mutex_lock(&floor_mutexes[from]);
  if (request_counts[from] < MAX_REQUESTS) {
    floor_buffers[from][request_counts[from]++] = req;
    printf("Requisição: Passageiro %d solicitou do andar %d para o andar %d\n",
           req.passenger_id, from, to);
    total_requests++;
  } else {
    printf("Requisição: Passageiro %d não pôde ser adicionada ao andar %d "
           "(fila cheia)\n",
           req.passenger_id, from);
  }
  pthread_mutex_unlock(&floor_mutexes[from]);
}

int main() {
  pthread_t elevators[MAX_ELEVATORS];
  int elevator_ids[MAX_ELEVATORS];

  for (int i = 0; i < MAX_FLOORS; i++) {
    sem_init(&floor_semaphores[i], 0, 0);
    pthread_mutex_init(&floor_mutexes[i], NULL);
  }

  for (int i = 0; i < MAX_ELEVATORS; i++) {
    elevator_ids[i] = i + 1;
    pthread_create(&elevators[i], NULL, elevator, &elevator_ids[i]);
  }

  srand(time(NULL));

  while (total_requests < MAX_REQUESTS) {
    make_request();
    sleep(rand() % 4 + 1);
  }

  for (int i = 0; i < MAX_ELEVATORS; i++) {
    pthread_join(elevators[i], NULL);
  }

  for (int i = 0; i < MAX_FLOORS; i++) {
    pthread_mutex_destroy(&floor_mutexes[i]);
    sem_destroy(&floor_semaphores[i]);
  }

  printf("Todos os elevadores finalizaram suas requisições.\n");

  return 0;
}
