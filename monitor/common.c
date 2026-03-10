#include <string.h>

#include "common.h"

/*
 * 1. magic number
 * 2. handle
 * 3. cpu_usage
 */

// TODO: handle endianness
void Serialize(uint8_t *dest, MonitorData *src)
{
  uint32_t m = MAGIC_NUMBER;
  size_t offset = 0;

  memcpy(dest + offset, &m, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  memcpy(dest + offset, &src->handle, sizeof(int32_t));
  offset += sizeof(int32_t);

  memcpy(dest + offset, &src->cpu_usage, sizeof(float));
}

// TODO: handle endianness
int Deserialize(MonitorData *dest, uint8_t *src)
{
  uint32_t m;
  size_t offset = 0;

  memcpy(&m, src + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  if (m != MAGIC_NUMBER)
  {
    return -1;
  }

  memcpy(&dest->handle, src + offset, sizeof(int32_t));
  offset += sizeof(int32_t);

  memcpy(&dest->cpu_usage, src + offset, sizeof(float));

  return 0;
}


