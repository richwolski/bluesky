/* generic_hash.c: Generic hash function that hashes on a maximum of three keys
 * $Id: generic_hash.c,v 1.5 2002/08/21 22:08:01 ningning Exp $
 * Changes:
 * 
 *	$Log: generic_hash.c,v $
 *	Revision 1.5  2002/08/21 22:08:01  ningning
 *	*** empty log message ***
 *	
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generic_hash.h"

/* create an entry for a block */
void generic_insert(char * key1, unsigned int key1_size, unsigned int key3, struct generic_entry **htable, int hsize)
{
unsigned int index;
struct generic_entry *p,*q;

unsigned int * tmp =(unsigned int *)key1;
unsigned int i;
index = 0;
for (i=0; i<(key1_size/4); i++) 
	index += *(tmp+i);
index = index%hsize;
{
//printf ("generic_insert index %d key %s\n", index, key1);
}
p = q = htable[index];
while (p) {
    if ((!memcmp(p->key1, key1, key1_size))
        && (p->key3==key3)) {
        return;
    }
    q = p;
    p = p->next;
}
if (!p) {
    p = (struct generic_entry *)malloc(sizeof(struct generic_entry));
    memcpy(p->key1, key1, key1_size);
    p->key3 = key3;
    p->next = NULL;
}
if (!q)
    htable[index] = p;
else
    q->next = p;
return;
}

void generic_delete(char * key1, unsigned int key1_size, unsigned int key3, struct generic_entry **htable, int hsize)
{
unsigned int index;
int found;
struct generic_entry *p,*q;

unsigned int * tmp =(unsigned int *)key1;
unsigned int i;
index = 0;
for (i=0; i<(key1_size/4); i++) 
	index += *(tmp+i);
index = index%hsize;

p = q = htable[index];
found = 0;
while (p) {
    if ((!memcmp(p->key1, key1, key1_size)) 
	    && (p->key3==key3)) {
		found = 1;
		break;
    }
	q = p;
    p = p->next;
}
RFS_ASSERT(found==1);
if (p==htable[index])
    htable[index] = p->next;
else 
    q->next = p->next;

/* free hash entry */

free(p);

return;
}

struct generic_entry *generic_lookup(char * key1, unsigned int key1_size, unsigned int key3, struct generic_entry **htable, int hsize)
{
unsigned int index;
struct generic_entry *p;

unsigned int * tmp =(unsigned int *)key1;
unsigned int i;
index = 0;
for (i=0; i<(key1_size/4); i++) 
	index += *(tmp+i);
index = index%hsize;
{
//printf ("generic_lookup index %d key %s\n", index, key1);
}

p = htable[index];
while (p) {
    if (!memcmp(p->key1, key1, key1_size)) 
	{
		return p;
    }
    p = p->next;
}
/*RFS_ASSERT(0);*/
return NULL;
}

struct generic_entry *generic_lookup1(unsigned int key1, unsigned int key2, unsigned int key3, struct generic_entry **htable, int hsize)
{
unsigned int index;
struct generic_entry *p;

index = ((unsigned int)key1)%hsize;
p = htable[index];
while (p) {
    if (p->key1==key1) 
		return p;
    p = p->next;
}
/*RFS_ASSERT(0);*/
return NULL;
}

struct generic_entry *generic_lookup2(unsigned int key1, unsigned int key2, unsigned int key3, struct generic_entry **htable, int hsize)
{
unsigned int index;
struct generic_entry *p;

index = (key1 + key2)%hsize;
p = htable[index];
while (p) {
    if (   (p->key1==key1) 
	    && (p->key2==key2)
	   )
		return p;
    p = p->next;
}
/*RFS_ASSERT(0);*/
return NULL;
}

void generic_display(struct generic_entry **htable, int hsize, int numkeys)
{
int i;
struct generic_entry *p;
int counter = 0;

for (i=0;i<hsize;i++) {
    p = htable[i];
	if (p) 
		printf ("htable[%d]", i);
    while (p) {
		if (numkeys==1)
			printf("%d, ", p->key1);
		else if (numkeys==2)
			printf("(%d,%x), ", p->key1,p->key2);
		else 
			printf("(%d,%d,%d), ", p->key1,p->key2,p->key3);
	    p = p->next;
        counter++;
		if ((counter%4)==3) 
		    printf("\n");
    }
}
printf("\ncounter was %d\n",counter);
}

/* create an entry for a block */
void generic_long_insert4(unsigned int key1, unsigned int key2, unsigned int key3, unsigned int key4, unsigned int key5, unsigned int key6, struct generic_long_entry **htable, int hsize)
{
int index;
struct generic_long_entry *p,*q;

index = (key1 + key2 + key3 + key4)%hsize;
p = q = htable[index];
while (p) {
    if ((p->key1==key1) 
	    && (p->key2==key2)
	    && (p->key3==key3)
	    && (p->key4==key4)
	    && (p->key5==key5)
	    && (p->key6==key6)) {
		return;
    }
	q = p;
    p = p->next;
}
if (!p) {
	p = (struct generic_long_entry *)malloc(sizeof(struct generic_long_entry));
	p->key1 = key1;
	p->key2 = key2;
	p->key3 = key3;
	p->key4 = key4;
	p->key5 = key5;
	p->key6 = key6;
    p->next = NULL;
}
if (!q) 
    htable[index] = p;
else 
    q->next = p;
return;
}

struct generic_entry *generic_long_lookup4(unsigned int key1, unsigned int key2, unsigned int key3, unsigned int key4, struct generic_long_entry **htable, int hsize)
{
int index;
struct generic_long_entry *p;

index = (key1 + key2 + key3 + key4 )%hsize;
p = htable[index];
while (p) {
    if (   (p->key1==key1) 
	    && (p->key2==key2)
		&& (p->key3==key3)
		&& (p->key4==key4)
	   )
		return p;
    p = p->next;
}
/*RFS_ASSERT(0);*/
return NULL;
}
