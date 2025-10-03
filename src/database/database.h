#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <libpq-fe.h>

bool init_db_vars();

PGconn *init_db_conn();

void close_db_conn();

bool init_pool();
void free_pool();

PGconn* get_conn();

void release_conn(PGconn *conn);

#endif
