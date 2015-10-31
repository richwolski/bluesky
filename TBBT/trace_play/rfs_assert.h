#ifndef RFS_ASSERT_H
#define RFS_ASSERT_H
#define RFS_ASSERT(condition)                                       \
    if (!(condition)) {                                             \
        fprintf(stderr, "Assertion failed: line %d, file \"%s\"\n", \
	                    __LINE__, __FILE__);                        \
		fflush(stdout); \
		fflush(stdout); \
		fflush(stderr); \
		fflush(stderr); \
		exit(-1); \
    }
#endif
