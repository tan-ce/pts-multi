#ifndef _BCRYPT_H_
#define _BCRYPT_H_

#include <sys/types.h>

char   *bcrypt_gensalt(u_int8_t log_rounds);
char   *bcrypt(const char *key, const char *salt);

#endif /* _BCRYPT_H_ */
