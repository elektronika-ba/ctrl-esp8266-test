#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "driver/uart.h"

#include "ctrl_database.h"

tNode *ctrlDatabase = NULL;
unsigned long gTXbase = 1; // wee need this variable because we are not going to keep all sent+acknowledged messages in database like we do on Server implementation

/*
	This database model is used to store outgoing messages from this Base -> Server.
	All incoming messages from Server -> Base (us) are processed immediatelly when received.
	If this chip can't process them upon arrival, it should set Backoff and Server will
	re-send later.
	Backoff should be set BEFORE the message is recevied by CTRL stack or the message
	will not be re-sent by the Server. This means that this Base should know in advance
	whether it can or can't process the next message it will receive from Server.
*/

void ICACHE_FLASH_ATTR ctrl_database_ack_row(unsigned long TXbase)
{
	// mark THIS message as acked. also, remove it from QUEUE since there is no point in holding it anymore BUT ONLY IF THERE ARE NO UNACKED TRANSMISSIONS OLDER THAN IT!
	// we use global TXbase variable to keep track of next TXbase to assign for next row to add so it is quite safe to remove them

	unsigned char canFree = 1;
	tNode *tmp = ctrlDatabase;
	while(tmp != NULL)
	{
		if((tmp->row)->TXbase == TXbase)
		{
			(tmp->row)->acked = 1;

			//char stmp[50];
			//os_sprintf(stmp, "DB acked on TXbase = %u\r\n", TXbase);
			//uart0_sendStr(stmp);

			break;
		}
		else if((tmp->row)->acked == 0)
		{
			canFree = 0;
		}
		tmp = tmp->next;
	}

	if(canFree)
	{
		ctrl_database_delete_by_TXbase(TXbase);
	}
}

// returns next database row from database, and marks it as SENT
tDatabaseRow * ICACHE_FLASH_ATTR ctrl_database_get_next_txbase2server(void)
{
	//uart0_sendStr("DB selecting next: ");

	tNode *tmp = ctrlDatabase;
	while(tmp != NULL)
	{
		if((tmp->row)->sent == 0 && (tmp->row)->acked == 0)
		{
			(tmp->row)->sent = 1;

			//char stmp[50];
			//os_sprintf(stmp, "TXbase = %u\r\n", (tmp->row)->TXbase);
			//uart0_sendStr(stmp);

			return tmp->row;
		}
		tmp = tmp->next;
	}

	//uart0_sendStr("NO NEXT!\r\n");

	return NULL;
}

void ICACHE_FLASH_ATTR ctrl_database_unsend_all(void)
{
	//uart0_sendStr("DB mark all as unsent:");
	tNode *tmp = ctrlDatabase;
	while(tmp != NULL)
	{
		//char stmp[20];
		//os_sprintf(stmp, " %u", (tmp->row)->TXbase);
		//uart0_sendStr(stmp);

		(tmp->row)->sent = 0;
		tmp = tmp->next;
	}
	//uart0_sendStr(".\r\n");
}

// returns: 1 on error, 0 on success
unsigned char ICACHE_FLASH_ATTR ctrl_database_add_row(char *data, unsigned short len)
{
	if(ctrl_database_count() >= CTRL_DATABASE_CAPACITY)
	{
		return 1; // no more memory
	}

	tDatabaseRow *row = (tDatabaseRow *)os_malloc(sizeof(tDatabaseRow));
	row->TXbase = gTXbase;
	row->data = (char *)os_malloc(len);
	os_memcpy(row->data, data, len);
	row->len = len;
	row->sent = 0;
	row->acked = 0;

	// Increment TXbase to be assigned because to next row... We flush the queue during operation and we need to keep track of last TXbase we've sent out
	gTXbase++;

	//char stmp[50];
	//os_sprintf(stmp, "DB added new row TXbase = %u\r\n", gTXbase-1);
	//uart0_sendStr(stmp);

	return ctrl_database_add_node(row);
}

// flushing acknowledged messages until we get to the first unacknowledged (or 'till the end), then we stop
void ICACHE_FLASH_ATTR ctrl_database_flush_acked(void)
{
	//uart0_sendStr("DB flush acked:");

	tNode *tmp = ctrlDatabase;
	while(tmp != NULL)
	{
		if((tmp->row)->acked == 1)
		{
			//char stmp[20];
			//os_sprintf(stmp, " %u", (tmp->row)->TXbase);
			//uart0_sendStr(stmp);

			tmp = ctrl_database_delete_by_TXbase((tmp->row)->TXbase); // returns the element which pointed to the deleted one so we can continue

			// this should never be NULL in this loop, but who knows...
			if(tmp == NULL)
			{
				//uart0_sendStr(".\r\n");
				return;
			}
		}
		else
		{
			break;
		}

		tmp = tmp->next;
	}
	//uart0_sendStr(".\r\n");
}

// count number of elements in database
static unsigned char ICACHE_FLASH_ATTR ctrl_database_count(void)
{
	unsigned char count = 0;

	tNode *tmp = ctrlDatabase;
	while(tmp != NULL)
	{
		count++;
		tmp = tmp->next;
	}

	return count;
}

// count unacked items from DB
unsigned char ICACHE_FLASH_ATTR ctrl_database_count_unacked_items(void)
{
	unsigned char count = 0;

	tNode *tmp = ctrlDatabase;
	while(tmp != NULL)
	{
		if((tmp->row)->acked == 0)
		{
			count++;
		}
		tmp = tmp->next;
	}

	//char stmp[50];
	//os_sprintf(stmp, "DB count unacked: %u\r\n", count);
	//uart0_sendStr(stmp);

	return count;
}

void ICACHE_FLASH_ATTR ctrl_database_delete_all(void)
{
	tNode *pointer = ctrlDatabase;
	tNode *next;

	//uart0_sendStr("DB deleting all.\r\n");

	while(pointer != NULL)
	{
		next = pointer->next;

		if((pointer->row)->data != NULL)
		{
			os_free((pointer->row)->data);
		}
		os_free(pointer->row);
		os_free(pointer);

		//if(next != NULL)
		//{
			pointer = next;
		//}
	}

	/*gTXbase = 1;
	ctrlDatabase = NULL;*/

	ctrl_database_init();
}

// deletes entry in database by row
static tNode * ICACHE_FLASH_ATTR ctrl_database_delete_by_TXbase(unsigned long TXbase)
{
	// Database empty?
	if(ctrlDatabase == NULL)
	{
		return NULL;
	}

	tNode *pointer = ctrlDatabase;
	tNode *temp;

	// if found at the first location, don't seek it
	if((pointer->row)->TXbase == TXbase)
	{
		temp = ctrlDatabase;

		ctrlDatabase = temp->next;
		pointer = temp->next;
	}
	else
	{
		// Go to the node for which the node next to it has to be deleted
		while(pointer->next != NULL && ((pointer->next)->row)->TXbase != TXbase)
		{
			pointer = pointer->next;
		}

		// Element not found in list
		if(pointer->next == NULL)
		{
			return pointer;
		}

		temp = pointer->next;
		pointer->next = temp->next;
	}

	// temp holds the element that must be os_free()-ed

	// free the memory
	if((temp->row)->data != NULL)
	{
		os_free((temp->row)->data);
	}
	os_free(temp->row);
	os_free(temp);

	//char stmp[50];
	//os_sprintf(stmp, "DB deleted TXbase = %u\r\n", TXbase);
	//uart0_sendStr(stmp);

	return pointer;
}

// add database row to the list
static unsigned char ICACHE_FLASH_ATTR ctrl_database_add_node(tDatabaseRow *row)
{
	// create new row
	tNode *newListItem = (tNode *)os_malloc(sizeof(tNode));
	if(newListItem == NULL)
	{
		return 1; // no more memory!
	}
	newListItem->row = (tDatabaseRow *)os_malloc(sizeof(tDatabaseRow));
	if(newListItem->row == NULL)
	{
		return 1; // no more memory!
	}
	os_memcpy(newListItem->row, row, sizeof(tDatabaseRow));
	newListItem->next = NULL;

	tNode *last = (tNode *)ctrl_database_find_last();
	// list is totally empty?
	if(last == NULL)
	{
		ctrlDatabase = newListItem;
	}
	// add to end of the list
	else
	{
		last->next = newListItem;
	}

	return 0;
}

// return address of last element in list, or NULL if list is empty
static tNode * ICACHE_FLASH_ATTR ctrl_database_find_last()
{
	if(ctrlDatabase == NULL)
	{
		return NULL;
	}

	tNode *last = ctrlDatabase;
	while(last->next != NULL)
	{
		last = last->next;
	}

	return last;
}

void ICACHE_FLASH_ATTR ctrl_database_init()
{
	ctrlDatabase = NULL;
	gTXbase = 1;
}
