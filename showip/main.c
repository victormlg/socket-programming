#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char **argv)
{
  struct addrinfo hints, *res, *p;
  int status;
  char ipstr[INET6_ADDRSTRLEN];

  if (argc != 2)
  {
    printf("Usage: showip hostname\n");
    return 1;
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(argv[argc-1], NULL, &hints, &res);

  if (status != 0)
  {
    return status;
  }

  printf("IP addresses for %s:\n\n", argv[1]);
  for (p = res; p != NULL; p = p->ai_next)
  {
    void *addr;
    char *ipver;
    struct sockaddr_in *ipv4;
    struct sockaddr_in6 *ipv6;
    if (p->ai_family == AF_INET)
    {
      ipv4 = (struct sockaddr_in *) p->ai_addr;
      addr = &ipv4->sin_addr;
      ipver = "IPv4";
    }
    else if (p->ai_family == AF_INET6) 
    {
      ipv6 = (struct sockaddr_in6 *) p->ai_addr;
      addr = &ipv6->sin6_addr;
      ipver = "IPv6";
    }
    else {
      printf("something else\n");
      continue;
    }

    inet_ntop(p->ai_family, addr, ipstr, INET6_ADDRSTRLEN);
    printf("- %s: %s\n", ipver, ipstr);
  }

  freeaddrinfo(res);
  return 0;
}
