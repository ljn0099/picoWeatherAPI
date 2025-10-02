#ifndef ROUTER_H
#define ROUTER_H

#include "server.h"

typedef struct yyguts_t *yyscan_t;
typedef struct yy_buffer_state *YY_BUFFER_STATE;

int yylex(yyscan_t scanner);
int yylex_init_extra(struct HandlerContext *ctx, yyscan_t *scanner);
int yylex_destroy(yyscan_t scanner);

YY_BUFFER_STATE yy_scan_string(const char *str, yyscan_t scanner);
void yy_delete_buffer(YY_BUFFER_STATE buffer, yyscan_t scanner);

#endif
