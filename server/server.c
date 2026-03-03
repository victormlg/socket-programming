#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>

#define PORT "6000"
#define BACKLOG 10

void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int main()
{
  char *errhdr = "Server Error";
  int sockfd, new_fd;
  int yes = 1;
  int ret = 0;
  struct addrinfo hints, *servinfo, *p;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((ret = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "%s getaddrinfo: %s\n", errhdr, gai_strerror(ret));
    goto error;
  }

  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      continue;
    }
    if ((ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) != 0)
    {
      fprintf(stderr, "%s setsockopt: failure\n", errhdr);
      goto error;
    }
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) != 0)
    {
      close(sockfd);
      continue;
    }
    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL)
  {
    fprintf(stderr, "%s: failed to bind\n", errhdr);
    goto error;
  }

  if ((ret = listen(sockfd, BACKLOG)) != 0)
  {
    fprintf(stderr, "%s listen: failure\n", errhdr);
    goto error;
  }

  struct sigaction sa;
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if ((ret = sigaction(SIGCHLD, &sa, NULL)) != 0) {
    fprintf(stderr, "%s sigaction: failure\n", errhdr);
    goto error;
  }

  struct sockaddr_storage their_addr;
  socklen_t addr_size = sizeof(struct sockaddr_storage);

  printf("Server waiting for connections...\n");
  while (1)
  {

    new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &addr_size);
    if (new_fd == -1) {
        fprintf(stderr, "%s accept: failure\n", errhdr);
        continue;
    }

    if (!fork())
    {
      close(sockfd);
      if (send(new_fd, "Hello world!", 13, 0) == -1)
      {
        fprintf(stderr, "%s send: failure\n", errhdr);
      }
      close(new_fd);
      exit(0);
    }
    close(new_fd);
  }
  
error:
  close(sockfd);
  return ret;
}
