#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#define FILE_NUM 25000
#define DIR_NUM 1
#define FILE_SIZE 40960

int print_usage()
{
	printf("init_holder DIR_NUM HOLDER_NUM HOLDER_SIZE testdir\n");
}

main4(int argc, char ** argv)
{
	int i, j;
	char name[256];
	char cmd[1024];
	char buf[FILE_SIZE];
	int fd, ret;
	char testdir[1024];

	if (argc!=5) {
		print_usage();
		exit (-1);
	}
	if (DIR_NUM !=1) {
		for (i=0; i<DIR_NUM; i++) {
			memset (name, 0, sizeof(name));
			sprintf(name, "%s/%d", testdir, i);
 			ret = mkdir (name, S_IRWXU);
			if (ret == -1) {
				perror(name);
				exit(-1);
			}
		}
	}

	for (j=0; j<DIR_NUM; j++) {
		for (i=0; i<FILE_NUM/DIR_NUM; i++) {
			if (DIR_NUM == 1)  
				sprintf(name, "%s/%d", testdir, i);
			else
				sprintf(name, "%s/%d/%d", testdir, j, i);
			fd = open (name, O_CREAT|O_WRONLY);
			if (fd == -1) {
				perror(name);
				exit(-1);
			}
			ret = write (fd, buf, FILE_SIZE );
			close (fd);
			if (ret!=FILE_SIZE) {
				printf("try to write %d , get %d\n", SIZE, ret);
				exit (-1);
			}
		}
	}
}
