#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>

#include "common.h"

#define BUFFER_SIZE 512
#define SLEEP_TIME 1
#define SERVER_IP_PATH "/tmp/mlg-monitor-ip"


// ############ Monitoring Logic ############ 

struct cpu_time {
  uint64_t total;
  uint64_t idle;
};

static void Move(struct cpu_time *t1, struct cpu_time *t2)
{
  t1->idle = t2->idle;
  t1->total = t2->total;
}

static int ReadProcStat(char *buffer, struct cpu_time *c)
{
  FILE *f = fopen("/proc/stat", "r");

  if (f == NULL)
  {
    syslog(LOG_ERR, "Couldn't open /proc/stat: %m");
    return -1;
  }

  if (fgets(buffer, BUFFER_SIZE, f) == NULL)
  {
    syslog(LOG_ERR, "Couldn't read /proc/stat: %m");
    fclose(f);
    return -1;
  }
  fclose(f);

  bool is_terminated = false;
  char *ptr = buffer;
  uint64_t total = 0;
  uint64_t idle = 0;

  int column = 0;

  while (ptr <= buffer + BUFFER_SIZE)
  {
    if (isdigit(*ptr))
    {
      uint64_t value = strtoll(ptr, &ptr, 10);
      if (column == 3 || column == 4)
      {
        idle += value;
      }
      total += value;
      column += 1;
    }
    if (*ptr == '\n')
    {
      is_terminated = true;
    }
    ptr++;
  }

  c->total = total;
  c->idle = idle;

  if (is_terminated)
  {
    return 0;
  }

  syslog(LOG_ERR, "Truncated line in /proc/stat");
  return -1;
}

static float CalculateCPUUsage(struct cpu_time *t1, struct cpu_time *t2)
{
  assert(t2->total > t1->total);
  assert(t2->idle > t1->idle);
  uint64_t total_delta = t2->total - t1->total;
  uint64_t idle_delta = t2->idle - t1->idle;

  if (total_delta == 0)
  {
    return 0.0f;
  }

  float cpu_usage = (double) (total_delta - idle_delta) / total_delta * 100;
  syslog(LOG_DEBUG, "CPU Usage: %.2f%%", cpu_usage);
  return cpu_usage;
}

// ############ Connection Logic ############

static int ReadIP(char *buffer)
{
  FILE *f = fopen(SERVER_IP_PATH, "r");

  if (f == NULL)
  {
    return -1;
  }

  if (fgets(buffer, BUFFER_SIZE, f) == NULL) {
    fclose(f);
    return -1;
  }
  fclose(f);

  size_t len = strlen(buffer);
  if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
      len--;
  }

  if (len == 0) return -1;
  return 0;
}

static int InitUDPConnection(Connection *conn, char *ip)
{
  int sockfd, ret;
  struct addrinfo hints, *server_info, *p;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if ((ret = getaddrinfo(ip, SERVER_PORT, &hints, &server_info)) != 0)
  {
    syslog(LOG_ERR, "Couldn't get address info: %s", gai_strerror(ret));
    return -1;
  }

  for (p = server_info; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
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

static int SendData(Connection *conn, MonitorData *data)
{
  int ret = 0;
  uint8_t binary_data[PROT_SIZE];
  Serialize(binary_data, data);

  if ((ret = sendto(conn->sockfd, binary_data, PROT_SIZE, 0, (struct sockaddr *) &conn->addr, conn->addr_len)) == -1)
  {
    return -1;
  }
  syslog(LOG_DEBUG, "Sent %d bytes", ret);

  return 0;
}

int GetHandle(char *ip, MonitorData *data)
{
  int sockfd, ret;
  struct addrinfo hints, *server_info, *p;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((ret = getaddrinfo(ip, HANDLE_PORT, &hints, &server_info)) != 0)
  {
    syslog(LOG_ERR, "Couldn't get address info: %s", gai_strerror(ret));
    return -1;
  }

  for (p = server_info; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      syslog(LOG_DEBUG, "Socket error: %m");
      continue;
    }
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      syslog(LOG_DEBUG, "Connect error: %m");
      close(sockfd);
      continue;
    }
    break;
  }

  if (p == NULL)
  {
    syslog(LOG_ERR, "Couldn't establish a connection via TCP");
    return -1;
  }

  freeaddrinfo(server_info);

  uint8_t binary_data[PROT_SIZE];
  if (recv(sockfd, binary_data, PROT_SIZE, 0) == -1) {
    syslog(LOG_ERR, "Failed while receiving data");
    close(sockfd);
    return -1;
  }
  if (Deserialize(data, binary_data) != 0)
  {
    syslog(LOG_ERR, "Couldn't deserialize packet");
    close(sockfd);
    return -1;
  }

  if (data->handle == -1)
  {
    syslog(LOG_ERR, "No free handle avalaible");
    return -1;
  }

  syslog(LOG_INFO, "Received handle '%d'", data->handle);
  
  close(sockfd);
  return 0;
}


// ############ Main logic ############

static void DaemonStart()
{
  pid_t pid;
  pid = fork();
  
  if (pid < 0)
    exit(EXIT_FAILURE);
  
  if (pid > 0)
    exit(EXIT_SUCCESS);
  
  if (setsid() < 0)
    exit(EXIT_FAILURE);

  // TODO: Implement a working signal handler
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  
  pid = fork();

  if (pid < 0)
    exit(EXIT_FAILURE);
  if (pid > 0)
    exit(EXIT_SUCCESS);
  
  umask(0);
  chdir("/");
  int x;
  for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
  {
    close(x);
  }

  openlog("mlg-monitor-client", LOG_PID, LOG_DAEMON);
}

int main()
{
  DaemonStart();
  syslog(LOG_INFO, "Starting mlg-monitor-client");

  char buffer[BUFFER_SIZE];
  struct cpu_time t1, t2;

  if (ReadProcStat(buffer, &t1) != 0)
  {
    syslog(LOG_ERR, "Fatal: Failed to read /proc/stat");
    goto cleanup;
  }

  Connection conn;
  if (ReadIP(buffer) != 0)
  {
    syslog(LOG_ERR, "Fatal: Couldn't read server IP at" SERVER_IP_PATH);
    goto cleanup;
  }

  MonitorData data;
  if (GetHandle(buffer, &data) != 0)
  {
    syslog(LOG_ERR, "Fatal: Unable to get handle");
    goto cleanup;
  }

  if (InitUDPConnection(&conn, buffer) != 0)
  {
    syslog(LOG_ERR, "Fatal: Couldn't initialize network");
    goto cleanup;
  }

  while (1)
  {
    sleep(SLEEP_TIME);
    if (ReadProcStat(buffer, &t2) != 0)
    {
      syslog(LOG_ERR, "Fatal: Failed to read /proc/stat");
      break;
    }
    data.cpu_usage = CalculateCPUUsage(&t1, &t2);

    if(SendData(&conn, &data) != 0)
    {
      syslog(LOG_WARNING, "Failed to send monitoring data");
    }
    Move(&t1, &t2);
  }

  close(conn.sockfd);
cleanup:
  closelog();
  return 0;
}

