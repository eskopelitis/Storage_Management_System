#ifndef NET_H_
#define NET_H_

#include <stdint.h>
#include <stdbool.h>

#define HEADER_LEN (sizeof(uint32_t) + sizeof(uint8_t))
#define JBOD_SERVER "127.0.0.1"
#define JBOD_PORT 4104

int jbod_client_operation(uint32_t op, uint8_t *block);
bool jbod_connect(const char *ip, uint16_t port);
void jbod_disconnect(void);

#endif
