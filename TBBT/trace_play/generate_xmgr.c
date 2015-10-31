#include <stdio.h>
#include "rfs_assert.h"

#define EVEN_CHUNK_SIZE_MAX 2
#define CHUNK_NUM_MAX 17
#define FS_SIZE_MB_MAX 81
#define STAGE_NUM_MAX 65
typedef	struct {
	int flag;
	int frag_num;
	int avg_frag_distance;
	int avg_block_distance;
} result_t;
result_t result[EVEN_CHUNK_SIZE_MAX][CHUNK_NUM_MAX][FS_SIZE_MB_MAX][STAGE_NUM_MAX];

int range_check (int i, int min, int max)
{
	if (i<min)
		return 0;
	if (i>max)
		return 0;
	return 1;
}

int file_ratio, active_ratio, chunk_num, fs_size_MB, even_chunk_size, stage_num;
#define SEQ_NUM 11
int frag_num[SEQ_NUM], td_MB[SEQ_NUM], td[SEQ_NUM], tblock_num[SEQ_NUM], file_num;
main(int argc, char ** argv)
{
	char line[1024];
	FILE * fp;
	unsigned long long distance;
	unsigned long long tll;
	int avg_frag_distance = 0;
	int max_blockno=0, avg_block_distance=0;
	char * p;
	int sequence[SEQ_NUM]={1,2,5,10,20,30,50,100,200,250,500};
	int i;
	char fhmap[1024];
	result_t * resultp= NULL;


	memset (result, 0, sizeof(result));

	fp = fopen (argv[1], "r");
	if (!fp) {
		perror("open");	
		exit(-1);
	}
	i=-1;
	while (fgets(line, sizeof(line), fp)) {
		if (feof(fp))
			break;
		if (strstr(line, "==>")) {
			RFS_ASSERT (resultp == NULL);

			p = strrchr (line, '/');
			if (p==NULL)
				p=line;
			p = strchr (p, '_');
			if (p==NULL) {
				printf("error 2\n");
				exit(-1);
			}
			sscanf (p, "_%d_%d_%d_%d_%d_%d_%s.5 <==\n", &even_chunk_size, &file_ratio, &active_ratio, &chunk_num, &fs_size_MB, &stage_num, fhmap);
			i = chunk_num;
			RFS_ASSERT (range_check (even_chunk_size, 0, EVEN_CHUNK_SIZE_MAX-1));
			RFS_ASSERT (range_check (chunk_num, 0, CHUNK_NUM_MAX-1));
			RFS_ASSERT (range_check (fs_size_MB/100, 0, FS_SIZE_MB_MAX-1));
			RFS_ASSERT (range_check (stage_num, 0, STAGE_NUM_MAX-1));
			resultp = & (result[even_chunk_size][chunk_num][fs_size_MB/100][stage_num]);
			RFS_ASSERT (resultp->flag == 0);
		}
		if (strstr(line, "****")) {

			RFS_ASSERT (resultp);

			sscanf(line, "****total FRAG_NUM %d td_MB %d td %d tblock_num %d max_blockno %d file_num %d avg_frag_distance %d avg_block_distance %d", &frag_num[0], &td_MB[0], &td[0],  &tblock_num[0], &max_blockno, &file_num, &avg_frag_distance, &avg_block_distance);
			sscanf(line, "****total FRAG_NUM %d td_MB %d td %d tblock_num %d max_blockno %d file_num %d avg_frag_distance %d avg_block_distance %d", &(resultp->frag_num), &td_MB[0], &td[0],  &tblock_num[0], &max_blockno, &file_num, &(resultp->avg_frag_distance), &(resultp->avg_block_distance));
			resultp ->flag = 1;

			//printf("%d %d %d %d %d %d\n", chunk_num, frag_num[0], avg_frag_distance, td_MB[0], tblock_num[0], avg_block_distance);

			resultp = NULL;
		} 
	}

	print_xmgr ("avg_block_distance");
	print_xmgr ("frag_num");
	print_xmgr ("frag_size");
	print_xmgr ("avg_frag_distance");
	print_xmgr_chunk_stage ("avg_block_distance");
}

int print_y(FILE * fp, result_t * resultp, char * y_axis)
{
	if (!strcmp (y_axis, "avg_block_distance")) {
		fprintf (fp, "%d ", resultp->avg_block_distance);
	} else if (!strcmp (y_axis, "frag_num")) {
		fprintf (fp, "%d ", resultp->frag_num);
	} else if (!strcmp (y_axis, "frag_size")) {
		fprintf (fp, "%f ", (float)tblock_num[0]/(float)resultp->frag_num);
	} else if (!strcmp (y_axis, "avg_frag_distance")) {
		fprintf (fp, "%d ", resultp->avg_frag_distance);
	} else {
		RFS_ASSERT (0);
	}
}
	/* statistics for avg_block_distance */
int print_xmgr (char * y_axis)
{
	int flag1, flag2;
	result_t * resultp;
	char name[1024];

	FILE * fp = NULL;

	for (even_chunk_size = 0; even_chunk_size < EVEN_CHUNK_SIZE_MAX; even_chunk_size ++) {
		for (fs_size_MB = 0; fs_size_MB < FS_SIZE_MB_MAX; fs_size_MB ++) {

			sprintf (name, "xmgr.%d_%d.stage.%s", even_chunk_size, fs_size_MB*100, y_axis);
			fp = NULL;
			flag1 = 1;
			for (stage_num = 0; stage_num < STAGE_NUM_MAX; stage_num++) {
				flag2 = 1;
				for (chunk_num = 0; chunk_num < CHUNK_NUM_MAX; chunk_num++) {
					resultp = &(result[even_chunk_size][chunk_num][fs_size_MB][stage_num]);
					if (resultp->flag) {
						if (flag1) {
							printf ("*** even %d fs_size_MB %d X: stage_num Y: %s Lines: different chunk_num ***\n", even_chunk_size, fs_size_MB*100, y_axis);
							flag1 = 0;
							if (!fp) {
								fp = fopen (name, "w");
								if (!fp)
									RFS_ASSERT (0);
							}
						}
						if (flag2) {
							//fprintf (fp, "%d ", file_num/stage_num);
							fprintf (fp, "%f ", 1/((float)stage_num));
							flag2 = 0;
						}
						print_y (fp, resultp, y_axis);
					}
				}
				if (!flag2)
					fprintf (fp, "\n");
			}
			if (fp)
				fclose (fp);
		}
	}

	for (even_chunk_size = 0; even_chunk_size < EVEN_CHUNK_SIZE_MAX; even_chunk_size ++) {
		for (fs_size_MB = 0; fs_size_MB < FS_SIZE_MB_MAX; fs_size_MB ++) {
			sprintf (name, "xmgr.%d_%d.chunk.%s", even_chunk_size, fs_size_MB*100, y_axis);
			fp = NULL;
			flag1 = 1;
			for (chunk_num = 0; chunk_num < CHUNK_NUM_MAX; chunk_num++) {
				flag2 = 1;
				for (stage_num = 0; stage_num < STAGE_NUM_MAX; stage_num++) {
					resultp = &(result[even_chunk_size][chunk_num][fs_size_MB][stage_num]);
					if (resultp->flag) {
						if (flag1) {
							printf ("*** even %d fs_size_MB %d X: chunk_num Y: %s Lines: different stage_num ***\n", even_chunk_size, fs_size_MB*100, y_axis);
							flag1 = 0;
							if (!fp) {
								fp = fopen (name, "w");
								if (!fp)
									RFS_ASSERT (0);
							}
						}
						if (flag2) {
							fprintf (fp, "%d ", chunk_num);
							flag2 = 0;
						}
						print_y (fp, resultp, y_axis);
					}
				}
				if (!flag2)
					fprintf (fp, "\n");
			}
			if (fp)
				fclose (fp);
		}
	}
}
int print_xmgr_chunk_stage (char * y_axis)
{
	int flag1, flag2;
	result_t * resultp;
	char name[1024];

	FILE * fp = NULL;

	for (even_chunk_size = 0; even_chunk_size < EVEN_CHUNK_SIZE_MAX; even_chunk_size ++) {
		for (fs_size_MB = 0; fs_size_MB < FS_SIZE_MB_MAX; fs_size_MB ++) {

			sprintf (name, "xmgr.%d_%d.stage_chunk.%s", even_chunk_size, fs_size_MB*100, y_axis);
			fp = NULL;
			flag1 = 1;
			for (stage_num = STAGE_NUM_MAX-1; stage_num>=0; stage_num--) {
				flag2 = 1;
				for (chunk_num = 0; chunk_num < CHUNK_NUM_MAX; chunk_num++) {
					resultp = &(result[even_chunk_size][chunk_num][fs_size_MB][stage_num]);
					if (resultp->flag) {
						if (flag1) {
							printf ("*** even %d fs_size_MB %d X: stage_num Y: %s Lines: different chunk_num ***\n", even_chunk_size, fs_size_MB*100, y_axis);
							flag1 = 0;
							if (!fp) {
								fp = fopen (name, "w");
								if (!fp)
									RFS_ASSERT (0);
							}
						}
						fprintf (fp, "%f ", chunk_num/((float)stage_num));
						print_y (fp, resultp, y_axis);
						fprintf (fp, "\n");
					}
				}
			}
			if (fp)
				fclose (fp);
		}
	}
}
