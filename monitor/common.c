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

  memcpy(dest, &m, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  memcpy(dest + offset, &src->handle, sizeof(src->handle));
  offset += sizeof(src->handle);

  memcpy(dest + offset, &src->cpu_usage, sizeof(src->cpu_usage));
}

// TODO: handle endianness
int Deserialize(MonitorData *dest, uint8_t *src)
{
  uint32_t m;
  size_t offset = 0;

  memcpy(&m, src, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  if (m != MAGIC_NUMBER)
  {
    return -1;
  }

  memcpy(&dest->handle, src + offset, sizeof(dest->handle));
  offset += sizeof(dest->handle);

  memcpy(&dest->cpu_usage, src + offset, sizeof(dest->cpu_usage));

  return 0;
}


