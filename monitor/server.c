#include <time.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pthread.h>

#include "common.h"

#define MAX_CLIENTS 100

typedef struct {
  MonitorData data;
  time_t last_seen;
} RemoteClient;

RemoteClient client_table[MAX_CLIENTS];

static void ClientTableWrite(MonitorData *data)
{
  client_table[0].last_seen = time(NULL);
  memcpy(&client_table[0].data, data, sizeof(MonitorData));
}

static int InitServer(Connection *conn)
{
  int sockfd, ret;
  struct addrinfo hints, *server_info, *p;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  if ((ret = getaddrinfo(NULL, SERVER_PORT, &hints, &server_info)) != 0)
  {
    fprintf(stderr, "Couldn't get address info: %s\n", gai_strerror(ret));
    return -1;
  }

  for (p = server_info; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      continue;
    }
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      continue;
    }

    conn->sockfd = sockfd;
    conn->addr_len = p->ai_addrlen;
    memcpy(&conn->addr, p->ai_addr, p->ai_addrlen);
    break;
  }

  freeaddrinfo(server_info);
  return (p == NULL) ? -1 : 0;
}

static int ReceiveData(Connection *conn, MonitorData *data)
{
  uint8_t binary_data[PROT_SIZE];

  int ret = recvfrom(conn->sockfd, binary_data, PROT_SIZE, 0, (struct sockaddr *) &conn->addr, &conn->addr_len);

  if(Deserialize(data, binary_data) != 0)
  {
    return -1;
  }

  return 0;
}

void *PrintTable()
{
  while (1)
  {
    time_t now = time(NULL);

    RemoteClient r = client_table[0];
    printf("\r[%d] CPU: %2.f%%, status: %s", 0, r.data.cpu_usage, (now - r.last_seen < 10) ? "online" : "offline");
    fflush(stdout);
    sleep(1);
  }
  return NULL;
}

int main()
{
  Connection conn;
  if (InitServer(&conn) != 0)
  {
    fprintf(stderr, "Failed to initialize server\n");
  }

  pthread_t thread;
  pthread_create(&thread, NULL, PrintTable, NULL);

  MonitorData data;
  while (1)
  {
    if (ReceiveData(&conn, &data))
    {
      fprintf(stderr, "Error while receiving data\n");
    }

    ClientTableWrite(&data);
  }

  pthread_cancel(thread);
  pthread_join(thread, NULL);
  close(conn.sockfd);
  return 0;
}

