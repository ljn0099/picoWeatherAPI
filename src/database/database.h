#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>
#include <stdbool.h>

bool init_db_vars(void);

bool init_pool(void);
void free_pool(void);

PGconn *get_conn(void);

void release_conn(PGconn *conn);

#endif
