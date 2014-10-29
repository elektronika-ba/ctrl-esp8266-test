#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

#include "ctrl_stack.h"
#include "ctrl_database.h"
#include "driver/uart.h"

tNode *ctrlDatabase = NULL;

// deletes entry in database by row
void ICACHE_FLASH_ATTR ctrl_database_delete(tDatabaseRow *row)
{
	tNode *pointer = ctrlDatabase;

	// Go to the node for which the node next to it has to be deleted
	while(pointer != NULL && pointer->next != NULL && (pointer->next)->row != row)
	{
		pointer = pointer->next;
	}

	// element not found in list
	if(pointer->next == NULL)
	{
		return;
	}

	// Now pointer points to a node and the node next to it has to be removed
	tNode *temp = pointer->next;

	// We remove the node which is next to the pointer (which is also temp)
	pointer->next = temp->next;

	// free the memory
	os_free(temp->row);
	os_free(temp);
}

// add database row to the list
unsigned char ICACHE_FLASH_ATTR ctrl_database_add(tDatabaseRow *row)
{
	tNode *last = (tNode *)ctrl_database_find_last();

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

	// list is empty?
	if(last == NULL)
	{
		ctrlDatabase = newListItem;
	}
	// add to end of the list
	else
	{
		last->next = newListItem;
	}
}

// return address of wanted element, or NULL if list is empty
tNode * ICACHE_FLASH_ATTR ctrl_database_find_by_TXsender(unsigned long TXsender)
{
	tNode *found = ctrlDatabase;

	while(found != NULL)
	{
		if(((found->row)->msg).TXsender == TXsender)
		{
			break;
		}
		found = found->next;
	}

	return found;
}

// return address of next element, or NULL if list is empty
tNode * ICACHE_FLASH_ATTR ctrl_database_find_last()
{
	tNode *last = ctrlDatabase;

	while(last != NULL)
	{
		last = last->next;
	}

	return last;
}

void ICACHE_FLASH_ATTR ctrl_database_init()
{

}
