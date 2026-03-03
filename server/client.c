#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define PORT "6000"

int main()
{
  char *errhdr = "Server Error";
  struct addrinfo hints, *clientinfo, *p;
  int ret = 0;
  int sockfd, new_fd;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((ret = getaddrinfo(NULL, PORT, &hints, &clientinfo)) != 0) {
    fprintf(stderr, "%s getaddrinfo: %s\n", errhdr, gai_strerror(ret));
    goto error;
  }

  for (p = clientinfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      continue;
    }
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) != 0)
    {
      close(sockfd);
      continue;
    }
    break;
  }
  
  freeaddrinfo(clientinfo);

  if (p == NULL)
  {
    fprintf(stderr, "%s: failed to bind\n", errhdr);
    goto error;
  }
  printf("Client connected\n");

  char buf[32];
  if ((ret = recv(sockfd, buf, 99, 0)) == 0)
  {
    ret = 1;
    goto error;
  }

  buf[ret] = '\0';

  printf("Client received: '%s'\n", buf);

error:
  close(sockfd);

  return 0;
}
