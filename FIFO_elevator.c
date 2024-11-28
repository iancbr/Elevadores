#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_REQUESTS 10
#define MAX_ELEVATORS 2

typedef struct {
  int from_floor;
  int to_floor;
} Request;

Request request_queue[MAX_REQUESTS];
int request_count = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t request_available = PTHREAD_COND_INITIALIZER;

sem_t elevator_ready; // Semáforo para controlar o início dos elevadores

void move_elevator(int *current_floor, int target_floor, int elevator_id) {
  while (*current_floor != target_floor) {
    if (*current_floor < target_floor) {
      (*current_floor)++;
    } else {
      (*current_floor)--;
    }

    if (*current_floor == target_floor) {
      printf("Elevador %d: Chegou ao andar %d\n", elevator_id, *current_floor);
    } else {
      printf("Elevador %d: Passando pelo andar %d\n", elevator_id,
             *current_floor);
    }

    sleep(1); // Simula o tempo de movimento
  }
}

void *elevator(void *id) {
  int elevator_id = *((int *)id);
  int current_floor = 0;

  // Espera pelo semáforo ser liberado para começar a processar as requisições
  sem_wait(&elevator_ready);

  while (1) {
    pthread_mutex_lock(&queue_mutex);

    while (request_count == 0) {
      pthread_cond_wait(&request_available,
                        &queue_mutex); // Espera por requisição
    }

    Request req = request_queue[0]; // FIFO: Pega a primeira requisição
    for (int i = 0; i < request_count - 1; i++) {
      request_queue[i] = request_queue[i + 1]; // Move fila para a esquerda
    }
    request_count--;

    pthread_mutex_unlock(&queue_mutex);

    printf("Elevador %d: Movendo do andar %d para o andar %d\n", elevator_id,
           current_floor, req.from_floor);
    move_elevator(&current_floor, req.from_floor, elevator_id);

    printf("Elevador %d: Movendo do andar %d para o andar %d\n", elevator_id,
           current_floor, req.to_floor);
    move_elevator(&current_floor, req.to_floor, elevator_id);
  }

  return NULL;
}

void make_request(int from, int to) {
  pthread_mutex_lock(&queue_mutex);
  if (request_count < MAX_REQUESTS) {
    Request req = {from, to};
    request_queue[request_count++] = req;
    printf("Requisição: Do andar %d para o andar %d\n", from, to);
    pthread_cond_signal(&request_available); // Notifica os elevadores
  } else {
    printf("Fila de requisições cheia!\n");
  }
  pthread_mutex_unlock(&queue_mutex);
}

int main() {
  pthread_t elevators[MAX_ELEVATORS];
  int elevator_ids[MAX_ELEVATORS];

  // Inicializa o semáforo com 0, para que os elevadores fiquem bloqueados até
  // liberar
  sem_init(&elevator_ready, 0, 0);

  // Cria as threads dos elevadores
  for (int i = 0; i < MAX_ELEVATORS; i++) {
    elevator_ids[i] = i + 1;
    pthread_create(&elevators[i], NULL, elevator, &elevator_ids[i]);
  }

  // Entrada de requisições: escolha quantas e quais
  int num_requests;
  while (1) {
    printf("Quantas requisições você deseja fazer? (Máximo %d): ",
           MAX_REQUESTS);
    if (scanf("%d", &num_requests) != 1 || num_requests <= 0 ||
        num_requests > MAX_REQUESTS) {
      printf("Número de requisições inválido. Tente novamente.\n");
      while (getchar() != '\n')
        ; // Limpa o buffer
      continue;
    }

    // Loop para ler as requisições do usuário
    for (int i = 0; i < num_requests; i++) {
      int from, to;
      printf("Digite a %dª requisição no formato 'origem,destino': ", i + 1);
      while (getchar() != '\n')
        ; // Limpa o buffer de novas linhas

      if (scanf("%d,%d", &from, &to) != 2) {
        printf("Erro na leitura da requisição. Tente novamente.\n");
        break;
      }

      if (from == to) {
        printf("O andar de origem não pode ser o mesmo que o destino.\n");
      } else if (from > 0 && to > 0) {
        make_request(from, to);
      } else {
        printf("Os andares devem ser positivos.\n");
      }

      // Se o número máximo de requisições for atingido, sai do loop
      if (request_count >= MAX_REQUESTS) {
        printf("Limite de requisições atingido.\n");
        break;
      }
    }

    // Pergunta se o usuário quer adicionar mais requisições
    char choice;
    printf("Deseja adicionar mais requisições? (s/n): ");
    while (getchar() != '\n')
      ; // Limpa o buffer
    choice = getchar();
    if (choice != 's' && choice != 'S') {
      break;
    }
  }

  // Libera o semáforo para os elevadores começarem a processar as requisições
  sem_post(&elevator_ready);

  // Junta as threads (nunca vai parar nesse exemplo simplificado)
  for (int i = 0; i < MAX_ELEVATORS; i++) {
    pthread_join(elevators[i], NULL);
  }

  // Destroi o semáforo
  sem_destroy(&elevator_ready);

  return 0;
}
