#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pthread.h>
#include <assert.h>

#include "common.h"

#define MAX_CLIENTS 100

typedef struct {
  MonitorData data;
  time_t last_seen;
  bool in_use;
} RemoteClient;

RemoteClient client_table[MAX_CLIENTS];

static void InitClientTable()
{
  for (size_t i = 0; i < MAX_CLIENTS; i++)
  {
    client_table[i].last_seen = 0;
    client_table[i].in_use = false;
  }
}

static void ClientTableWrite(MonitorData *data)
{
  assert(data->handle < MAX_CLIENTS);

  client_table[data->handle].last_seen = time(NULL);
  memcpy(&client_table[data->handle].data, data, sizeof(MonitorData));
}

static int32_t GetFreeHandle()
{
  for (size_t i = 0; i < MAX_CLIENTS; i++)
  {
    if (!client_table[i].in_use)
    {
      client_table[i].in_use = true;
      return i;
    }
  }
  return -1;
}

// ############ Connection Logic ############

static int InitUDPServer(Connection *conn)
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

int InitTCPServer(Connection *conn)
{
  int sockfd, ret;
  struct addrinfo hints, *server_info, *p;
  int yes = 1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((ret = getaddrinfo(NULL, HANDLE_PORT, &hints, &server_info)) != 0)
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
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      return -1;
    }
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      continue;
    }
    break;
  }

  if (p == NULL)
  {
    fprintf(stderr, "Unable to setup connection\n");
    return -1;
  }

  if (listen(sockfd, 10) == -1)
  {
    fprintf(stderr, "Unable to listen\n");
    return -1;
  }

  memcpy(&conn->addr, p->ai_addr, p->ai_addrlen);
  conn->addr_len = p->ai_addrlen;
  conn->sockfd = sockfd;
  freeaddrinfo(server_info);
  return 0;
}

void *GiveHandle(void *arg)
{
  Connection *conn = (Connection *) arg;
  int new_fd;

  while (1)
  {
    new_fd = accept(conn->sockfd, (struct sockaddr *) &conn->addr, &conn->addr_len);
    if (new_fd == -1)
    {
      continue;
    }

    MonitorData data;
    data.handle = GetFreeHandle();
    uint8_t binary_data[PROT_SIZE];
    Serialize(binary_data, &data);

    send(new_fd, binary_data, PROT_SIZE, 0);
    close(new_fd);
  }
}

void *PrintTable()
{
  while (1)
  {
    for (size_t i = 0; i < MAX_CLIENTS; i++)
    {
      RemoteClient r = client_table[i];

      time_t now = time(NULL);
      if (now - r.last_seen >= 10)
      {
        client_table[i].in_use = false;
      }

      if (r.in_use)
      {
        printf("[%ld] CPU: %.2f%%, status: %s\n", i, r.data.cpu_usage, (now - r.last_seen < 5) ? "online" : "offline");
      }
    }
    printf("========\n");
    fflush(stdout);
    sleep(1);
  }
  return NULL;
}

int main()
{
  InitClientTable();
  Connection tcp, udp;
  
  if (InitTCPServer(&tcp) != 0)
  {
    fprintf(stderr, "Failed to initialize TCP server\n");
    return -1;
  }

  if (InitUDPServer(&udp) != 0)
  {
    fprintf(stderr, "Failed to initialize UDP server\n");
    return -1;
  }

  pthread_t print_table;
  pthread_create(&print_table, NULL, PrintTable, NULL);

  pthread_t give_handle;
  pthread_create(&give_handle, NULL, GiveHandle, (void *) &tcp);

  MonitorData data;
  while (1)
  {
    if (ReceiveData(&udp, &data) == -1)
    {
      fprintf(stderr, "Error while receiving data\n");
    }

    ClientTableWrite(&data);
  }

  pthread_cancel(print_table);
  pthread_join(print_table, NULL);

  pthread_cancel(give_handle);
  pthread_join(give_handle, NULL);
  close(udp.sockfd);
  close(tcp.sockfd);
  return 0;
}

