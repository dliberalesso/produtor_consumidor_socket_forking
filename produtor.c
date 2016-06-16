/* Produtor
 * Aluno: Douglas Liberalesso
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>

int randomNumber(int min, int max) {
  int result = 0, low = 0, high = 0;
  if (min < max) {
    low = min;
    high = max + 1;
  } else {
    low = max + 1;
    high = min;
  }

  result = (rand() % (high - low)) + low;
  return result;
}

int main(int argc, char const *argv[]) {
  int sockfd, portno, n, numero_convertido, i, j, vezes;
  struct sockaddr_in serv_addr;

  /* Se nao for especificada nenhum porta, vai ser 9000. */
  portno = (argc < 2) ? 9000 : atoi(argv[1]);

  /* Abre um socket */
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel abrir um socket.\n");
    exit(1);
  }

  /* Inicializa socket */
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(portno);

  /* Conecta com o servidor */
  if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel conectar ao servidor.\n");
    exit(1);
  }

  printf("Conectado.\n");

  numero_convertido = htonl(1); // 1 para produtor
  if(send(sockfd, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao servidor.\n");
    close(sockfd);
    exit(1);
  }

  srand(time(NULL));
  vezes = randomNumber(1, 4);
  printf("[PID: %d] Produzir: %d vezes\n", getpid(), vezes);

  numero_convertido = htonl(getpid()); // PID
  if(send(sockfd, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao servidor.\n");
    close(sockfd);
    exit(1);
  }

  numero_convertido = htonl(vezes); // vezes que vai produzir por ciclo
  if(send(sockfd, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
    fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem ao servidor.\n");
    close(sockfd);
    exit(1);
  }

  j = 1;
  while(1) {
    if(recv(sockfd, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
      fprintf(stderr, "ERRO: Nao foi possivel receber mensagem do servidor.\n");
      close(sockfd);
      exit(1);
    }

    n = ntohl(numero_convertido);
    if(n == 1) {
      for(i = 0; i < vezes; i++) {
        if(recv(sockfd, &numero_convertido, sizeof(numero_convertido), 0) < 0) {
          fprintf(stderr, "ERRO: Nao foi possivel receber mensagem do servidor.\n");
          close(sockfd);
          exit(1);
        }

        n = ntohl(numero_convertido);
        if(n == 0) {
          printf("[PID: %d] Pilha encheu! O lote %d teve %d unidades\n", getpid(), j, i);
          break;
        } else {
          printf("[PID: %d] Produzindo: %d - Lote: %d\n", getpid(), ntohl(numero_convertido), j);
        }
      }
    } else {
      printf("[PID: %d] Cansei e nao vou mais produzir.\n", getpid());
      break;
    }
    j++;
  }

  printf("Desconectando...\n");
  close(sockfd);
  return 0;
}
