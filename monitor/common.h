
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define SERVER_PORT "6600"
#define PROT_SIZE 8
#define MAGIC_NUMBER 0xAB39FF

typedef struct {
  float cpu_usage;
} MonitorData;

void Serialize(uint8_t *dest, MonitorData *src);
int Deserialize(MonitorData *dest, uint8_t *src);

#endif
