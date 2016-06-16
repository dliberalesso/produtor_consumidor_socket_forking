/*
 * Aluno: Douglas Liberalesso
 */

#include <arpa/inet.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "pilha.h"

#define PROCESSOS 3
#define LIMITE 2048

typedef struct {
  sem_t p_sem;
  sem_t c_sem;
  int termina;
  Pilha pilha;
} Shared;

void produtor(int socket, Shared *shm) {
  int produzidos = 0;
  int pid_cliente, vezes, numero_convertido, i, j;

  /* Registra pid do cliente produtor */
  if(recv(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel receber mensagem do produtor.\n");
    close(socket);
    exit(1);
  } else {
    pid_cliente = ntohl(numero_convertido);
  }

  /* Registra quantas vezes vai produzir */
  if(recv(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel receber mensagem do produtor.\n");
    close(socket);
    exit(1);
  } else {
    vezes = ntohl(numero_convertido);
  }

  for(i = 1; i <= LIMITE; i++) {
    /* Aguarda ate semaforo ficar verde */
    sem_wait(&shm->p_sem);

    numero_convertido = htonl(1);
    if(send(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
      fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao produtor.\n");
      close(socket);
      exit(1);
    }

    /* Produz N produtos por ciclo */
    for(j = 0; j < vezes; j++) {
      if(!pilhaCheia(&shm->pilha)) {
        printf("[PRODUTOR: %d] Produzindo: %d - Lote: %d\n", pid_cliente, ++produzidos, i);
        empilha(&shm->pilha, produzidos);
        numero_convertido = htonl(produzidos);
        if(send(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
          fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao produtor.\n");
          close(socket);
          exit(1);
        }
      } else {
        printf("[PRODUTOR: %d] Pilha encheu! O lote %d teve %d unidades\n", pid_cliente, i, j);
        numero_convertido = htonl(0);
        if(send(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
          fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao produtor.\n");
          close(socket);
          exit(1);
        }
        break;
      }
    }

    /* Sinaliza que nao vai mais produzir caso tenha executado todos os ciclos */
    if(i == LIMITE) {
      shm->termina = 1;
      numero_convertido = htonl(0);
      if(send(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
        fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao produtor.\n");
        close(socket);
        exit(1);
      }
    }

    /* Libera consumidores */
    sem_post(&shm->c_sem);
  }

  close(socket);
}

void consumidor(int socket, Shared *shm) {
  int pid_cliente, vezes, numero_convertido, j;

  /* Registra pid do cliente produtor */
  if(recv(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel receber mensagem do consumidor.\n");
    close(socket);
    exit(1);
  } else {
    pid_cliente = ntohl(numero_convertido);
  }

  /* Registra quantas vezes vai produzir */
  if(recv(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel receber mensagem do consumidor.\n");
    close(socket);
    exit(1);
  } else {
    vezes = ntohl(numero_convertido);
  }

  while(1) {
    /* Se o produtor nao vai mais produzir, o proprio consumidor tem que liberar o seu semaforo */
    if(shm->termina) {
      sem_post(&shm->c_sem);
      /* Se a pilha ficar vazia e o produtor nao vai mais produzir, terminou a tarefa */
      if(pilhaVazia(&shm->pilha)) {
        numero_convertido = htonl(0);
        if(send(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
          fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao consumidor.\n");
          close(socket);
          exit(1);
        }
        break;
      }
    }

    /* Aguarda ate semaforo ficar verde */
    sem_wait(&shm->c_sem);

    numero_convertido = htonl(1);
    if(send(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
      fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao consumidor.\n");
      close(socket);
      exit(1);
    }

    /* Consome N vezes por ciclo */
    for(int j = 0; j < vezes; j++) {
      if(!pilhaVazia(&shm->pilha)) {
        printf("[CONSUMIDOR: %d] Consumindo: %d\n", pid_cliente, topoPilha(&shm->pilha));
        numero_convertido = htonl(topoPilha(&shm->pilha));
        if(send(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
          fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao consumidor.\n");
          close(socket);
          exit(1);
        }
        desempilha(&shm->pilha);
      } else {
        printf("[CONSUMIDOR: %d] Pilha esvaziou! Lote teve %d unidades\n", pid_cliente, j);
        numero_convertido = htonl(0);
        if(send(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
          fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao consumidor.\n");
          close(socket);
          exit(1);
        }
        break;
      }
    }

    /* Libera produtor */
    sem_post(&shm->p_sem);
  }
  close(socket);
}

void handle_comm(int socket, Shared *shm) {
  int status, numero_convertido;

  if(recv(socket, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel receber mensagem do cliente.\n");
    close(socket);
    exit(1);
  }

  if(ntohl(numero_convertido) == 1) {
    produtor(socket, shm);
  } else {
    consumidor(socket, shm);
  }
}

int main(int argc, char const *argv[]) {
  int sockfd, cli_socket, portno, clilen, pid, i;
  struct sockaddr_in serv_addr, cli_addr;

  Shared *shm = mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  sem_init(&shm->p_sem, 1, 1);
  sem_init(&shm->c_sem, 1, 0);
  shm->termina = 0;

  initPilha(&shm->pilha);

  /* Se nao for especificada nenhum porta, vai ser 9000. */
  portno = (argc < 2) ? 9000 : atoi(argv[1]);
  fprintf(stderr, "AVISO: Escutarei na porta %d.\n", portno);

  /* Abre um socket */
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel abrir um socket.\n");
    exit(1);
  }

  /* Inicializa socket */
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  /* Faz o bind do endereco host */
  if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "ERRO: Bind falhou.\n");
    exit(1);
  }

  /* Fica aguardando clientes */
  listen(sockfd, PROCESSOS);
  clilen = sizeof(cli_addr);

  /* Vamos esperar por tres cliente */
  for(i = 0; i < PROCESSOS; i++) {
    /* Aceita a conexao do cliente */
    if((cli_socket = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0) {
      fprintf(stderr, "ERRO: Nao foi possivel aceitar conexao do cliente.\n");
      break;
    }
    if((pid = fork()) < 0) {
      fprintf(stderr, "ERRO: Nao foi possivel criar um processo filho.\n");
      break;
    } else if(pid == 0) {
      close(sockfd);
      handle_comm(cli_socket, shm);
      exit(0);
    } else {
      close(cli_socket);
    }
  }

  for(i = 0; i < PROCESSOS; i++) {
    wait(NULL);
  }

  printf("FIM\n");
  return 0;
}
