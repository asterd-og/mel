#include <stdint.h>
uint64_t ToU64(void* v) {
  return (uint64_t)v;
}
uint32_t ToU32(void* v) {
  return (uint32_t)v;
}
uint16_t ToU16(void* v) {
  return (uint16_t)v;
}
uint8_t ToU8(void* v) {
  return (uint8_t)v;
}
void* ToPtr(void* v) {
  return (void*)v;
}