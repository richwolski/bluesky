#include <stdio.h>
#define RFS_ASSERT(condition)                                       \
    if (!(condition)) {                                             \
        fprintf(stderr, "Assertion failed: line %d, file \"%s\"\n", \
                        __LINE__, __FILE__);                        \
        fflush(stdout); \
        fflush(stderr); \
        exit(-1); \
    }

#define DISK_BLOCK_SIZE 4096
#define SUB_DISTANCE
int f()
{}
char buf[8024000];
main(int argc, char ** argv)
{
	FILE * fp;
	int ret;
	char * p;
	int i;
	int frag_num, distance, tfrag_num=0;
	int td=0, td_MB=0;
	int max_block_num = -1;
	int block_num = -1;
	int last_block_num;
	int tblock_num =0;
	int last_block = 0;
	int block_range_start, block_range_stop;
	int lineno = 0;
	int format_version = 1;
	char * p1=NULL, *p2=NULL, *p3=NULL;
	int size = -1;
	int max_block = 0;
	FILE * fpout;
	char name[1024];
	int file_num = 0;
	unsigned long long distancell = 0;
	int avg_frag_distance;
	int avg_block_distance;
#ifdef SUB_DISTANCE
#define MAX_SUB_DISTANCE_NUM 50
	int sub_distance_index;
	int SUB_DISTANCE_NUM = 1;
	int SUB_DISTANCE_SIZE = 1000000;
	unsigned long long sub_distance[MAX_SUB_DISTANCE_NUM];
	unsigned int sub_frag_num[MAX_SUB_DISTANCE_NUM], sub_block_num[MAX_SUB_DISTANCE_NUM];
	memset (&sub_distance, 0, sizeof(sub_distance));
	memset (&sub_frag_num, 0, sizeof(sub_frag_num));
	memset (&sub_block_num, 0, sizeof(sub_block_num));

	if (argc == 3) {
		SUB_DISTANCE_NUM = atoi(argv[2]);
		RFS_ASSERT ((SUB_DISTANCE_NUM >=1) && (SUB_DISTANCE_NUM <= MAX_SUB_DISTANCE_NUM));
	}
#endif

	fp = fopen (argv[1], "r");
    if (!fp) {
        printf ("can not opern %s\n", argv[1]);
        perror("open");
        exit (0);
    }


	strcpy (name, argv[1]);
	strcat (name, ".disk");

	fpout = fopen(name, "w");
	if (!fpout) {
        printf ("can not opern %s\n", name);
        perror("open");
        exit (0);
	}
	
	while (fgets(buf, sizeof(buf), fp)) {
		lineno++;
		if ((lineno%10000)==0) { // || (lineno >630000)) {
			fprintf(stderr, "%d lines processed\n", lineno);
		}
		if (lineno==122165)
			f();
		RFS_ASSERT (buf[strlen(buf)-1]=='\n');
		if (buf[0]=='U') {
			p = strstr (buf, "Size");
			RFS_ASSERT (p);
			if (size != -1) {
				printf ("lineno %d size %d\n", lineno, size);
			}
			RFS_ASSERT (size == -1);
			sscanf(p, "Size: %d", &size);
			continue;
		}


		/* For now we ignore symbolic links */
		if (!strncmp(buf, "Fast_link_dest", strlen("Fast_link_dest")))  {
			f();
			goto ENDLOOP;
		}

		if (buf[0]!='B')
			continue;

		RFS_ASSERT (!strcmp(buf, "BLOCKS:\n"));
		fgets(buf, sizeof(buf), fp);
		lineno++;
		if (!(buf[strlen(buf)-1]=='\n')) {
			printf ("line[%d] %s\n", lineno, buf);
		};
		RFS_ASSERT (buf[strlen(buf)-1]=='\n');
		if (!strcmp(buf, "\n"))
			goto ENDLOOP;


		RFS_ASSERT (size >= 0);
		RFS_ASSERT (block_num == -1);
		RFS_ASSERT (max_block_num == -1);
		block_num = 0; /* the block number of block_range_start of current fragment */
		last_block_num = 0; /* the block number of block_range_start of last fragment */
		max_block_num = 0;
		if (size >0) {
			char line[1024];
			int i;
			fgets(line, sizeof(line), fp);
			lineno++;
			RFS_ASSERT (line[strlen(line)-1]=='\n');
			RFS_ASSERT (strstr(line, "TOTAL: "));
			max_block_num = atoi (line+strlen("TOTAL: "));
			i = ((size+DISK_BLOCK_SIZE-1)/DISK_BLOCK_SIZE);
			RFS_ASSERT ((max_block_num >= i) && ((max_block_num*9/i)<10));
		}
		tblock_num += max_block_num;

		p = buf;
		frag_num = 0;
		distance = 0;
		last_block = 0;
		//printf ("line %d %s", lineno, buf);
		while (p && (*p!='\n')) {
			if (format_version == 1) {
				p1 = strchr (p, ')');
				if (p1) {
					p2 = strchr (p, '-');
					p3 = strchr (p, ':');
					RFS_ASSERT (p3);
					p3++;
				} else {
					format_version = 2;
					p3 = p;
				}
			} else
				p3 = p;

#define checkit
			/* single block range */
			if ((p2==NULL) || p2>p1) {	
#ifdef checkit
				char * pt2, a;
				pt2 = strchr(p3, ' ');
				if (!pt2) {
					pt2 = strchr(p3, '\n');
					a = '\n';
				} else
					a = ' ';
				RFS_ASSERT (pt2);
				RFS_ASSERT (pt2!=p3);
				*pt2 = 0;
				block_range_start = atoi (p3);
				*pt2 = a ;
#else
				sscanf(p3, "%d", &block_range_start);
#endif
				block_range_stop = block_range_start;
			} else {
#ifdef checkit
				char * pt, * pt2, a;
				pt = strchr(p3, '-');
				RFS_ASSERT (pt);
				*pt = 0;
				block_range_start = atoi (p3);
				*pt = '-';
				pt2 = strchr(pt+1, ',');
				if (!pt2) {
					pt2 = strchr(pt+1, '\n');
					a = '\n';
				} else
					a = ',';
				RFS_ASSERT (pt2);
				*pt2 = 0;
				block_range_stop = atoi (pt+1);
				*pt2 = a ;
#else
				sscanf(p3, "%d-%d", &block_range_start, &block_range_stop);
#endif
			}

			RFS_ASSERT (block_range_start >0);
			RFS_ASSERT (block_range_stop >= block_range_start);

			block_num += (block_range_stop - block_range_start+1);

			if (block_range_start != (last_block+1)) {
				frag_num ++;

#ifdef SUB_DISTANCE
				sub_distance_index = (block_num-1) * SUB_DISTANCE_NUM/max_block_num;
				sub_block_num[sub_distance_index] += block_num - last_block_num;
				last_block_num = block_num;
				//printf ("block_num %d SUB_DISTANCE_NUM %d max_block_num %d index %d\n", block_num, SUB_DISTANCE_NUM,
//max_block_num, sub_distance_index);
				RFS_ASSERT ((sub_distance_index>=0) && (sub_distance_index<SUB_DISTANCE_NUM));
#endif
				if (last_block!=0) {
					if (block_range_start > last_block+1) {
						distance += block_range_start - (last_block+1);
#ifdef SUB_DISTANCE
						sub_distance[sub_distance_index] += block_range_start - (last_block+1);
#endif
					} else {
						distance += (last_block+1)-block_range_start;
#ifdef SUB_DISTANCE
						sub_distance[sub_distance_index] += (last_block+1)-block_range_start ;
#endif
					}

					if (distance >= 1000000000) {
						printf ("line[%d] %s, block_range_start %d last_block %d\n", lineno, buf, block_range_start, last_block);
						RFS_ASSERT (0);
					}
					fprintf(fpout, "%d %d\n", last_block, block_range_start);
				};
				//printf ("range_start %d last_block %d distance %d\n", 
						//block_range_start, last_block, distance);
#ifdef SUB_DISTANCE
				sub_frag_num[sub_distance_index] ++;
#endif
			}
			
			last_block = block_range_stop;
			if (last_block > max_block)
				max_block = last_block;
			if (p1)
				p = strchr (p3, '(');
			else {
				p = strchr (p3, ' ');
				p++;
			}
		}
		//printf ("FRAG_NUM %d DISTANCE %d\n", frag_num, distance);
		tfrag_num += frag_num;
		distancell += distance;
ENDLOOP:
		file_num ++;
		size = -1;
		block_num = -1;
		max_block_num = -1;
/*
		td += distance;
		if (td > 1000000) {
			td_MB += td/1000000;
			td = td%1000000;
			RFS_ASSERT (td_MB < 1000000000);
		}
*/
	}
	fclose (fpout);
	fclose (fp);
                                                                              
	if (tfrag_num != file_num) {
		RFS_ASSERT ((distancell /(tfrag_num-file_num)) < 1000000000);
		RFS_ASSERT ((distancell /(tblock_num-file_num)) < 1000000000);
		avg_frag_distance = distancell/(tfrag_num-file_num);
		avg_block_distance = distancell/(tblock_num-file_num);
	} else {
		avg_frag_distance =0;
		avg_block_distance =0;
	}
	RFS_ASSERT ((distancell /1000000) < 1000000000);
	td_MB = distancell/1000000;
	td = distancell%1000000;
	
#ifdef SUB_DISTANCE
	for (i=0; i<SUB_DISTANCE_NUM; i++) {
		printf("sub[%d] block_num %d frag_num %d distance %d\n", i, sub_block_num[i], sub_frag_num[i], sub_distance[i]/1000000);
	}
#endif
/*
    distancell = td_MB;
    distancell *=1000000;
    distancell +=td;
    distancell /= (tfrag_num-file_num);
    if (distancell > 1000000000) {
        printf ("error 4\n");
        exit(-1);
    }
    avg_frag_distance = distancell;

    distancell = td_MB;
    distancell *=1000000;
    distancell +=td;
    distancell /= (tblock_num-file_num);
    if (distancell > 1000000000) {
        printf ("error 4\n");
        exit(-1);
    }
    avg_block_distance = distancell;
*/

	printf("****total FRAG_NUM %d td_MB %d td %d tblock_num %d max_blockno %d file_num %d avg_frag_distance %d avg_block_distance %d\n", tfrag_num, td_MB, td, tblock_num, max_block, file_num, avg_frag_distance, avg_block_distance);
}
