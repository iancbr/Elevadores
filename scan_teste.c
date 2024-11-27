#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_REQUESTS 3
#define MAX_ELEVATORS 2
#define MAX_FLOORS 10
#define MAX_PASSENGERS 3

typedef struct {
  int to_floor;
} Request;

Request floor_buffers[MAX_FLOORS][MAX_REQUESTS]; // Buffer por andar
int request_counts[MAX_FLOORS] = {0}; // Contador de requisições por andar
sem_t floor_semaphores[MAX_FLOORS]; // Semáforos para requisições em cada andar

void *elevator(void *arg) {
  int elevator_id = *((int *)arg);
  int current_floor = 0;
  int direction = 1; // 1 = subindo, -1 = descendo
  Request passengers[MAX_PASSENGERS];
  int passenger_count = 0;

  while (1) {
    // Procura o próximo andar com requisição
    int next_floor = -1;
    for (int i = 0; i < MAX_FLOORS; i++) {
      int floor_index =
          (current_floor + i * direction + MAX_FLOORS) % MAX_FLOORS;
      if (sem_trywait(&floor_semaphores[floor_index]) == 0) {
        next_floor = floor_index;
        break;
      }
    }

    if (next_floor == -1 &&
        passenger_count == 0) { // Nenhuma requisição e sem passageiros
      sleep(1);
      continue;
    }

    if (next_floor != -1) { // Movendo até o andar da próxima requisição
      printf("Elevador %d: Movendo do andar %d para o andar %d\n", elevator_id,
             current_floor, next_floor);
      while (current_floor != next_floor) {
        current_floor += (current_floor < next_floor) ? 1 : -1;
        printf("Elevador %d: Passando pelo andar %d\n", elevator_id,
               current_floor);
        sleep(2); // 2 segundos para mover de um andar para o outro
      }
      printf("Elevador %d: Chegou ao andar %d\n", elevator_id, current_floor);

      // Adiciona passageiros do andar atual até o limite
      int passengers_boarded = 0;
      while (passenger_count < MAX_PASSENGERS &&
             request_counts[next_floor] > 0) {
        passengers[passenger_count++] =
            floor_buffers[next_floor][0]; // Pega a requisição
        printf("Elevador %d: Passageiro com destino ao andar %d entrou\n",
               elevator_id, floor_buffers[next_floor][0].to_floor);
        for (int i = 0; i < request_counts[next_floor] - 1; i++) {
          floor_buffers[next_floor][i] = floor_buffers[next_floor][i + 1];
        }
        request_counts[next_floor]--;
        passengers_boarded++;
      }

      // Tempo para o elevador parar e pegar passageiros
      if (passengers_boarded > 0) {
        printf("Elevador %d: Parando no andar %d para embarque de %d "
               "passageiros\n",
               elevator_id, current_floor, passengers_boarded);
        sleep(4); // 4 segundos para parar e pegar passageiros
      }
    }

    // Atende os passageiros no elevador
    for (int i = 0; i < passenger_count;) {
      if (current_floor != passengers[i].to_floor) { // Movimenta para o destino
        current_floor += (current_floor < passengers[i].to_floor) ? 1 : -1;
        printf("Elevador %d: Passando pelo andar %d\n", elevator_id,
               current_floor);
        sleep(2); // 2 segundos para mover de um andar para o outro
      }
      if (current_floor == passengers[i].to_floor) { // Passageiro desembarca
        printf("Elevador %d: Passageiro desembarcou no andar %d\n", elevator_id,
               current_floor);
        for (int j = i; j < passenger_count - 1; j++) {
          passengers[j] = passengers[j + 1];
        }
        passenger_count--;
        printf("Elevador %d: Parando no andar %d para desembarque de "
               "passageiros\n",
               elevator_id, current_floor);
        sleep(4); // 4 segundos para parar e deixar o passageiro
      }
    }
  }

  return NULL;
}

void make_request() {
  if (request_counts[0] < MAX_REQUESTS) {
    // Gera andares aleatórios dentro do intervalo de 1 a MAX_FLOORS
    int from = rand() % MAX_FLOORS + 1;
    int to = rand() % MAX_FLOORS + 1;
    while (to == from) { // Evita que o andar de destino seja o mesmo
      to = rand() % MAX_FLOORS + 1;
    }

    Request req = {to};
    floor_buffers[from][request_counts[from]++] = req;
    printf("Requisição: Do andar %d para o andar %d\n", from, to);
    sem_post(&floor_semaphores[from]); // Sinaliza que há uma nova requisição
  } else {
    printf("Fila de requisições cheia no andar!\n");
  }
}

int main() {
  pthread_t elevators[MAX_ELEVATORS];
  int elevator_ids[MAX_ELEVATORS];

  // Inicializa semáforos
  for (int i = 0; i < MAX_FLOORS; i++) {
    sem_init(&floor_semaphores[i], 0, 0);
  }

  // Cria as threads dos elevadores
  for (int i = 0; i < MAX_ELEVATORS; i++) {
    elevator_ids[i] = i + 1;
    pthread_create(&elevators[i], NULL, elevator, &elevator_ids[i]);
  }

  // Inicializa a semente para números aleatórios
  srand(time(NULL));

  // Cria requisições aleatórias a cada 0 a 3 segundos
  for (int i = 0; i < MAX_REQUESTS; i++) { // Faz MAX_REQUESTS requisições
    make_request();
    sleep(rand() % 4); // Intervalo aleatório entre 0 a 3 segundos
  }

  // Junta as threads (nunca vai parar nesse exemplo simplificado)
  for (int i = 0; i < MAX_ELEVATORS; i++) {
    pthread_join(elevators[i], NULL);
  }

  // Libera recursos
  for (int i = 0; i < MAX_FLOORS; i++) {
    sem_destroy(&floor_semaphores[i]);
  }

  return 0;
}
