
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/socket.h>

#define SERVER_PORT "6600"
#define HANDLE_PORT "6599"

#define PROT_SIZE 8
#define MAGIC_NUMBER 0xAB39FF

typedef struct {
  int sockfd;
  struct sockaddr_storage addr;
  socklen_t addr_len;
} Connection;

typedef struct {
  float cpu_usage;
  int32_t handle;
} MonitorData;

void Serialize(uint8_t *dest, MonitorData *src);
int Deserialize(MonitorData *dest, uint8_t *src);

#endif
