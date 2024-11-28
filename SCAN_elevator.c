#include "timer.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_REQUESTS 7
#define MAX_ELEVATORS 3
#define MAX_FLOORS 50
#define MAX_PASSENGERS 4

// Códigos ANSI para cores
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

// Lista de cores fixas para passageiros
const char *passenger_colors[] = {RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN};
const int num_passenger_colors =
    sizeof(passenger_colors) / sizeof(passenger_colors[0]);

// Função para obter a cor fixa de um passageiro
const char *get_passenger_color(int passenger_id) {
  return passenger_colors[passenger_id % num_passenger_colors];
}

// Função para obter uma cor diferente para cada elevador
const char *get_elevator_color(int elevator_id) {
  switch (elevator_id) {
  case 1:
    return RED;
  case 2:
    return GREEN;
  case 3:
    return YELLOW;
  case 4:
    return BLUE;
  default:
    return WHITE;
  }
}

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

void *elevator(void *arg) {
  int elevator_id = *((int *)arg);
  int current_floor = 0;
  int direction = 1; // 1 = subindo, -1 = descendo
  Request passengers[MAX_PASSENGERS];
  int passenger_count = 0;

  while (1) {
    int no_requests = 1; // Indicador de ausência de requisições

    // Mostrar posição atual do elevador
    printf("%sElevador %d: No andar %d%s\n", get_elevator_color(elevator_id),
           elevator_id, current_floor, RESET);

    // Desembarcar passageiros no andar atual
    for (int i = 0; i < passenger_count; i++) {
      if (passengers[i].to_floor == current_floor) {
        printf("%sElevador %d: %sPassageiro %d%s desembarcou no andar %d%s\n",
               get_elevator_color(elevator_id), elevator_id,
               get_passenger_color(passengers[i].passenger_id),
               passengers[i].passenger_id, RESET, current_floor, RESET);

        for (int j = i; j < passenger_count - 1; j++) {
          passengers[j] = passengers[j + 1];
        }
        passenger_count--;
        i--;
        sleep(4); // Tempo para desembarque
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
        printf(
            "%sElevador %d: %sPassageiro %d%s entrou no elevador no andar %d "
            "com destino ao andar %d%s\n",
            get_elevator_color(elevator_id), elevator_id,
            get_passenger_color(floor_buffers[current_floor][i].passenger_id),
            floor_buffers[current_floor][i].passenger_id, RESET, current_floor,
            floor_buffers[current_floor][i].to_floor, RESET);

        for (int j = i; j < request_counts[current_floor] - 1; j++) {
          floor_buffers[current_floor][j] = floor_buffers[current_floor][j + 1];
        }
        request_counts[current_floor]--;
        i--;
        no_requests = 0; // Ainda existem requisições ativas
        sleep(4);
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
      printf("%sElevador %d: Não há mais requisições para atender.%s\n",
             get_elevator_color(elevator_id), elevator_id, RESET);
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
      printf("%sElevador %d: Mudando direção para %s%s\n",
             get_elevator_color(elevator_id), elevator_id,
             direction == 1 ? "subindo" : "descendo", RESET);
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
    printf("%sRequisição: %sPassageiro %d%s solicitou do andar %d para o andar "
           "%d%s\n",
           MAGENTA, get_passenger_color(req.passenger_id), req.passenger_id,
           RESET, from, to, RESET);
    total_requests++;
  } else {
    printf("%sRequisição: Passageiro %d não pôde ser adicionada ao andar %d "
           "(fila cheia)%s\n",
           MAGENTA, req.passenger_id, from, RESET);
  }
  pthread_mutex_unlock(&floor_mutexes[from]);
}

int main() {
  pthread_t elevators[MAX_ELEVATORS];
  int elevator_ids[MAX_ELEVATORS];
  double inicio, fim, delta;

  for (int i = 0; i < MAX_FLOORS; i++) {
    sem_init(&floor_semaphores[i], 0, 0);
    pthread_mutex_init(&floor_mutexes[i], NULL);
  }

  for (int i = 0; i < MAX_ELEVATORS; i++) {
    elevator_ids[i] = i + 1;
    pthread_create(&elevators[i], NULL, elevator, &elevator_ids[i]);
  }
  GET_TIME(inicio);
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

  printf("%sTodos os elevadores finalizaram suas requisições.%s\n", CYAN,
         RESET);
  GET_TIME(fim);
  delta = fim - inicio;
  printf("tempo do processo com %d elevadores, %d requisições: %f segundos\n",
         MAX_ELEVATORS, total_requests, delta);

  return 0;
}
