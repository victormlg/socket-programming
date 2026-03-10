#include <string.h>

#include "common.h"


// TODO: handle endianness
void Serialize(uint8_t *dest, MonitorData *src)
{
  uint32_t m = MAGIC_NUMBER;
  memcpy(dest, &m, sizeof(uint32_t));
  memcpy(dest + sizeof(uint32_t), &src->cpu_usage, sizeof(src->cpu_usage));
}

// TODO: handle endianness
int Deserialize(MonitorData *dest, uint8_t *src)
{
  uint32_t m;
  memcpy(&m, src, sizeof(uint32_t));

  if (m != MAGIC_NUMBER)
  {
    return -1;
  }

  memcpy(&dest->cpu_usage, src + sizeof(uint32_t), sizeof(dest->cpu_usage));

  return 0;
}


