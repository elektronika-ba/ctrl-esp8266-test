#ifndef __CTRL_DATABASE_H
#define __CTRL_DATABASE_H

#include "c_types.h"

// one database entry (row)
typedef struct {
	tCtrlMessage msg;
	unsigned char sent;
	unsigned char acked;
} tDatabaseRow;

// linked list item
typedef struct tnode {
	tDatabaseRow *row;
	struct tnode *next;
} tNode;

void ctrl_database_delete(tDatabaseRow *);
unsigned char ctrl_database_add(tDatabaseRow *);
tNode * ctrl_database_find_by_TXsender(unsigned long);
tNode * ctrl_database_find_last();
void ctrl_database_init();

#endif
