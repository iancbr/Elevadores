#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "timer.h"

#define MAX_REQUESTS 5
#define MAX_ELEVATORS 3
#define MAX_FLOORS 10

typedef struct {
  int from_floor;
  int to_floor;
  int passenger_id;
} Request;

Request request_queue[MAX_REQUESTS];
int request_count = 0;
int passenger_counter = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t requests_ready = PTHREAD_COND_INITIALIZER;

int inputs_completed = 0; // Indica que todas as requisições foram inseridas

void *elevator(void *id) {
  int elevator_id = *((int *)id);
  int current_floor = 0;

  while (1) {
    pthread_mutex_lock(&queue_mutex);

    // Espera até que haja requisições ou que todas as entradas tenham terminado
    while (request_count == 0 && !inputs_completed) {
      pthread_cond_wait(&requests_ready, &queue_mutex);
    }

    // Se não há mais requisições e entradas foram concluídas, encerrar
    if (request_count == 0 && inputs_completed) {
      pthread_mutex_unlock(&queue_mutex);
      break;
    }

    // Processa a primeira requisição na fila (FIFO)
    Request req = request_queue[0];
    for (int i = 0; i < request_count - 1; i++) {
      request_queue[i] =
          request_queue[i + 1]; // Desloca as requisições para frente
    }
    request_count--; // Decrementa a contagem de requisições

    pthread_mutex_unlock(&queue_mutex);

    // Movimentar até o andar de origem
    while (current_floor != req.from_floor) {
      current_floor += (current_floor < req.from_floor) ? 1 : -1;
      printf("Elevador %d: No andar %d\n", elevator_id, current_floor);
      sleep(1); // Simula tempo de movimentação
    }

    printf("Elevador %d: Passageiro %d entrou no elevador no andar %d com "
           "destino ao andar %d\n",
           elevator_id, req.passenger_id, req.from_floor, req.to_floor);
    sleep(4);

    // Movimentar até o andar de destino
    while (current_floor != req.to_floor) {
      current_floor += (current_floor < req.to_floor) ? 1 : -1;
      printf("Elevador %d: No andar %d\n", elevator_id, current_floor);
      sleep(1); // Simula tempo de movimentação
    }

    printf("Elevador %d: Passageiro %d desembarcou no andar %d\n", elevator_id,
           req.passenger_id, req.to_floor);
    sleep(4);
  }

  return NULL;
}

void make_request(int from, int to) {
  pthread_mutex_lock(&queue_mutex);
  if (request_count < MAX_REQUESTS) {
    Request req = {from, to, ++passenger_counter};
    request_queue[request_count++] = req;
    printf("Requisição: Passageiro %d solicitou do andar %d para o andar %d\n",
           req.passenger_id, from, to);
  } else {
    printf("Fila de requisições cheia!\n");
  }
  pthread_mutex_unlock(&queue_mutex);
}

int main() {
  pthread_t elevators[MAX_ELEVATORS];
  int elevator_ids[MAX_ELEVATORS];
  double inicio, fim, delta;

  for (int i = 0; i < MAX_ELEVATORS; i++) {
    elevator_ids[i] = i + 1;
    pthread_create(&elevators[i], NULL, elevator, &elevator_ids[i]);
  }

  GET_TIME(inicio);
  int num_requests;
  printf("Quantas requisições você deseja fazer? (Máximo %d): ", MAX_REQUESTS);
  if (scanf("%d", &num_requests) != 1) {
    fprintf(stderr, "Erro ao ler o número de requisições.\n");
    return 1;
  }

  for (int i = 0; i < num_requests; i++) {
    int from, to;
    printf("Digite a %dª requisição no formato 'origem,destino': ", i + 1);
    if (scanf("%d,%d", &from, &to) != 2) {
      fprintf(stderr, "Erro ao ler a requisição %d.\n", i + 1);
      return 1;
    }
    make_request(from, to);
  }

  pthread_mutex_lock(&queue_mutex);
  inputs_completed = 1; // Marca que todas as entradas foram finalizadas
  pthread_cond_broadcast(&requests_ready); // Acorda os elevadores
  pthread_mutex_unlock(&queue_mutex);

  // Espera todos os elevadores terminarem suas tarefas
  for (int i = 0; i < MAX_ELEVATORS; i++) {
    pthread_join(elevators[i], NULL);
  }

  // Mensagem final após o término de todas as requisições
  printf("Todos os elevadores finalizaram suas requisições.\n");

  GET_TIME(fim);
  delta = fim - inicio;
  printf("tempo do processo com %d elevadores, %d requisições: %f segundos\n",
         MAX_ELEVATORS, num_requests, delta);

  return 0;
}
