#ifndef __CTRL_DATABASE_H
#define __CTRL_DATABASE_H

#include "c_types.h"

// Define maximum database rows to store in total.
// Maximum is 255 because of "unsigned char" usage for this value. It can be
// changed though but the question is why having that long outgoing database
// table? What's wrong with the connection and why isn't it sending that data?
#define CTRL_DATABASE_CAPACITY		5

// one database entry (row)
typedef struct {
	//unsigned char notification;
	unsigned long TXbase;
	char *data;
	unsigned short len;

	unsigned char sent;
	unsigned char acked; // all acknowledged messages that have zero unacknowledged messages older than it self, should be removed from the database to free the memory. TXbase should be preserved in local variable of ctrl_database library because of that.
} tDatabaseRow;

// linked list item
typedef struct tnode {
	tDatabaseRow *row;
	struct tnode *next;
} tNode;

// private
static unsigned char ctrl_database_count(void);
static tNode * ctrl_database_delete_by_TXbase(unsigned long TXbase);
static unsigned char ctrl_database_add_node(tDatabaseRow *);
static tNode * ctrl_database_find_last();

// public
void ctrl_database_flush_acked(void);
void ctrl_database_ack_row(unsigned long);
void ctrl_database_unsend_all(void);
void ctrl_database_delete_all(void);
unsigned char ctrl_database_add_row(char *, unsigned short);
tDatabaseRow * ctrl_database_get_next_txbase2server(void);
unsigned char ctrl_database_count_unacked_items(void);
void ctrl_database_init();

#endif
