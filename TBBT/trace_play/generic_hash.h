#include "rfs_c_def.h"
struct generic_entry {
	char key1[MAX_TRACE_FH_SIZE];
    int key2;
    int key3;
    struct generic_entry *next;
};

struct generic_long_entry {
    int key1;
    int key2;
    int key3;
	int key4;
	int key5;
	int key6;
    struct generic_long_entry *next;
};

void generic_insert(char * key1, unsigned int key2, unsigned int key3, struct generic_entry **htable, int hsize);
void generic_delete(char * key1, unsigned int key2, unsigned int key3, struct generic_entry **htable, int hsize);
struct generic_entry *generic_lookup(char * key1, unsigned int key2, unsigned int key3, struct generic_entry **htable, int hsize);

void generic_display(struct generic_entry **htable, int hsize, int numkeys);

