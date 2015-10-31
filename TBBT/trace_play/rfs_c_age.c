/* rfs_age_unit_base.c */
#include <sys/vfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include "rfs_assert.h"
#include "profile.h"
#define MKDIR 1
#define RMDIR 2
#define CREATE 3
#define REMOVE 4
#define WRITE 5
#define TRUNCATE 6

#define MAX_FILES 400000
#define MAX_DIRS  100000
#define FILE_FH_HTABLE_SIZE MAX_FILES
#define MAX_NAMELEN 512
#define MAX_PLAY_PATH_SIZE 256
//#define MAX_COMMAND_LEN (MAX_PLAY_PATH_SIZE+16)
//#define NFS_MAXDATA 4096
#define NFS_MAXDATA 32768
#define TRACE_FH_SIZE 64

#define FH_T_FLAG_FREE 0
#define FH_T_FLAG_IN_USE 1
#define IS_FILE 0
#define IS_DIR 1
#define EXIST 0
#define NON_EXIST 1
#define COMPLETE 3
#define ACTIVE 0
#define INACTIVE 1
#define DONT_CARE 2

int EVEN_CHUNK_SIZE = 0;
int STAGE_NUM = 10;

static char ftypename[3][32] = {"FILE", "DIR", "FTYPE_DONT_CARE"};
static char activename[3][32] = {"ACTIVE", "INACTIVE", "ACTIVE_DONT_CARE"};
static char existname[4][32] = {"EXIST", "NON_EXIST", "EXIST_DONT_CARE", "COMPLETE"};

typedef struct {
    char flag;
	char ftype;
	char exist_flag;
	int psfh;
	int size;
	int cur_size;
	int accumulated_write_size;
    //char trace_fh [TRACE_FH_SIZE+1];
    char path[MAX_PLAY_PATH_SIZE];
} fh_t;

typedef struct {
	char name[32];
	fh_t * fh;
	//struct generic_entry * htable;
	int fh_size;
	int fh_max;
	int active_fh_max;
	//int index;
	//int htable_size;
} fh_info_t;

fh_info_t obj_fh;
profile_t read_line_profile, fgets_profile;
char trace_file[MAX_NAMELEN];
FILE * profile_fp = NULL;
char testdir[MAX_NAMELEN];

int active_obj_num = 0;
int exist_active_obj_num = 0;
static int active_file_num = 0, active_dir_num =0, age_file_num = 0, age_dir_num = 0;

int age_create_num = 0;
int age_mkdir_num = 0;
int assure_create_num = 0;
int assure_mkdir_num = 0;
int age_write_num = 0;
int nonage_write_num = 0;
int overlap_write_num = 0;

int fs_size_MB = 0, fs_size = 0;
int rfs_debug = 0;


int ACTIVE_RATIO;
int FILE_RATIO = 50;
int WRITE_CHUNK_NUM;
int MAX_FS_SIZE_MB = 1000000; 
int DISK_FRAGMENT_SIZE = 4096;
int DISK_FRAGMENT_SIZE_MASK = 0xFFFFF000;

int MIN_WRITE_SIZE;

int aging_dirs ()
{

}

int f()
{};

int init_profile_variables()
{
	init_profile ("read_line profile", &read_line_profile);
	init_profile ("fgets profile", &fgets_profile);
}

int init_fh_info (char * name, fh_info_t * fh_infop, int fh_size, int htable_size)
{
	int i;

	RFS_ASSERT (strlen(name) < sizeof(fh_infop->name));
	strcpy (fh_infop->name, name);
	fh_infop->fh_max = 0;
	//fh_infop->index = 0;
	fh_infop->fh_size = fh_size;
	//fh_infop->htable_size = htable_size;
	fh_infop->fh = (fh_t *)malloc (sizeof(fh_t)*fh_size);
	RFS_ASSERT (fh_infop->fh);
	//fh_infop->htable = malloc (sizeof(struct*generic_entry)*htable_size);
	//RFS_ASSERT (fh_infop->htable);
	printf("initialize %s size %d bytes\n", 
		//name, sizeof(fh_t)*fh_size + sizeof(struct*generic_entry)*htable_size);
		name, sizeof(fh_t)*fh_size);

	for (i=0; i<fh_size; i++)
		fh_infop->fh[i].flag = FH_T_FLAG_FREE;
}

int init()
{
//	init_fh_info ("file_fh", &file_fh, MAX_FILES, MAX_FILES);
//	init_fh_info ("dir_fh", &dir_fh, MAX_DIRS, MAX_DIRS);
	init_fh_info ("obj_fh", &obj_fh, MAX_FILES+MAX_DIRS, MAX_FILES+MAX_DIRS);
}

int add_fh_t (fh_info_t * fh_table, char * path, int sfh, int psfh, int size, int ftype, int exist_flag, int active_flag)
{
	int i;

	if (size == -1) 
		fs_size += DISK_FRAGMENT_SIZE;
	else {
		fs_size += ((size+DISK_FRAGMENT_SIZE-1) & DISK_FRAGMENT_SIZE_MASK);
		if (size > (DISK_FRAGMENT_SIZE*12))	// first indirect block
			fs_size += DISK_FRAGMENT_SIZE;
	}
	if (fs_size > 1000000) {
		fs_size_MB += fs_size/1000000;
		fs_size = fs_size % 1000000;
	}

	RFS_ASSERT (sfh >0);

	if (active_flag == ACTIVE)
		active_obj_num ++;
	else
		RFS_ASSERT (sfh >= fh_table->active_fh_max);

	if (rfs_debug)
		printf ("add to %s path %s sfh %d size %d %s %s %s\n", fh_table->name, path, sfh, size, 
		ftypename[ftype], existname[exist_flag], activename[active_flag]);

	RFS_ASSERT ( (sfh>=0) && (sfh<fh_table->fh_size) );
	RFS_ASSERT (fh_table->fh[sfh].flag==FH_T_FLAG_FREE);
	fh_table->fh[sfh].flag = FH_T_FLAG_IN_USE;
	if (sfh >= fh_table->fh_max)
		fh_table->fh_max = sfh+1;
	RFS_ASSERT (strlen(path)<MAX_PLAY_PATH_SIZE);
	strcpy (fh_table->fh[sfh].path, path);
	fh_table->fh[sfh].psfh = psfh;
	fh_table->fh[sfh].size = size;
	fh_table->fh[sfh].cur_size = 0;
	fh_table->fh[sfh].accumulated_write_size = 0;
	fh_table->fh[sfh].ftype = ftype;
	fh_table->fh[sfh].exist_flag = exist_flag;
	if (active_flag == ACTIVE) {
		if (ftype == IS_FILE)
			active_file_num ++;
		else {
			RFS_ASSERT (ftype== IS_DIR);
			active_dir_num ++;
		}
	} else {
		if (ftype == IS_FILE)
			age_file_num ++;
		else {
			RFS_ASSERT (ftype== IS_DIR);
			age_dir_num ++;
		}
	}
	//print_fh_map(fh_table);
}


int loop_write (int fd, char * buf, int buflen)
{
    int ret;
    int pos = 0;

    while (1) {
        ret = write (fd, buf+pos, buflen-pos);

        if (ret == -1) {
			RFS_ASSERT (errno == ENOSPC);
			fflush(stdout);
            perror ("loop write");
			check_free_blocks(0);
			return -1;
        }
        if (ret == buflen-pos)
            break;
        pos += ret;
    }
    return 0;
}

int assure_exist(int sfh, char * path, int ftype_flag)
{
	char name[MAX_NAMELEN];
	int ret;
	char *p, *q;
	int non_exist_flag = 0;
	int count=0;
	struct stat st;

	if (rfs_debug)
		printf("assure_exist %s\n", path);

	ret = stat (path, &st);
	if (ret == 0) {
		if (ftype_flag == IS_DIR) {
			RFS_ASSERT (st.st_mode & S_IFDIR);
			RFS_ASSERT (!(st.st_mode & S_IFREG));
		} else {
			RFS_ASSERT (st.st_mode & S_IFREG);
			RFS_ASSERT (!(st.st_mode & S_IFDIR));
		}
		return 0;
	}
	if (errno!=ENOENT) {
		perror(path);
	}
	//RFS_ASSERT (errno == ENOENT);
	
	p = path;
	q = name;
	if (*p=='/') {
		*q='/';
		p++;
		q++;
	}
	while (count++<100) {
		/* copy the next component from path to name */
		for (; *p!=0 && *p!='/'; p++, q++ ) 
			*q = *p;
		*q = 0;
		ret = stat (name, &st);
		if (ret == -1) {
			if (errno != ENOENT)
				perror (name);
			RFS_ASSERT (errno == ENOENT)
			if ((*p)==0 && (ftype_flag==IS_FILE)) {
				ret = creat (name, S_IRWXU);
				if (ret == -1)
					perror (name);
				RFS_ASSERT (ret >=0);
				assure_create_num ++;
				if (rfs_debug)
					printf("sfh %d create %s\n", sfh, name);
				close(ret);
			} else {
				ret = mkdir (name, S_IRWXU);
				if (ret == -1)
					perror (name);
				RFS_ASSERT (ret >=0);
				assure_mkdir_num ++;
				if (rfs_debug) {
					if (*p==0) 
						printf("sfh %d mkdir %s\n", sfh, name);
					else
						printf("sfh %d middle mkdir %s\n", sfh, name);
				}
				RFS_ASSERT (ret ==0);
			}
		}
		if ((*p)=='/') {
			*q = '/';
			p++; q++;
		} else {
			RFS_ASSERT ((*p)==0)
			return 0;
		}
	}
	RFS_ASSERT (0);
}


int print_fh_map(fh_info_t * fhp)
{
	int i;
	int num = 0;
	int active_obj_num = 0;


	for (i=0; i<fhp->fh_max; i++) {
		if (fhp->fh[i].flag == FH_T_FLAG_IN_USE) {
			num ++;
			if (i < fhp->active_fh_max)
				active_obj_num++;

			if (rfs_debug)
				printf("%s[%d] %s %s %s\n", fhp->name, i, fhp->fh[i].path, ftypename[fhp->fh[i].ftype], existname[fhp->fh[i].exist_flag]);
		}
	}
	fprintf(stderr, "fh_max %d active_fh_max %d, in_use_num %d entries active_obj_num %d \n", fhp->fh_max, fhp->active_fh_max, num, active_obj_num);
}

void read_fh_map_line_skimmer (char * buf)
{
	char * p;
	char name[MAX_NAMELEN];
	int psfh, sfh, size;
	char filename[MAX_NAMELEN];

	sfh = 0;
	if (!strncmp(buf, "::DIR ", strlen("::DIR "))) {
		strcpy (name, testdir);
		if (buf[6]=='/') {
			sscanf(buf, "::DIR %s %d\n", name+strlen(name), &sfh);
			add_fh_t (&obj_fh, name, sfh, -1, -1, IS_DIR, NON_EXIST, ACTIVE);
		} /* else { 
			RFS_ASSERT (!strncmp(buf,"::DIR Fake 1\n", strlen("::DIR Fake 1\n")));
			sfh = 1;
			add_fh_t (&obj_fh, name, sfh, -1, -1, IS_DIR, EXIST, ACTIVE);
			exist_active_obj_num ++;
		} */
	} else {

		if (!strncmp(buf, "::TBDIR", strlen("::TBDIR"))) 
			return; 

		p = strstr(buf, "parent");
		RFS_ASSERT (p);
		sscanf(p, "parent %d\n", &psfh);
		RFS_ASSERT (obj_fh.fh[psfh].flag == FH_T_FLAG_IN_USE);
		p = strstr(p, "name");
		RFS_ASSERT (p);
		if (!strncmp(p, "name xx", strlen("name xx"))) {
			sscanf(p, "name xx-%s sfh %d size %x", filename, &sfh, &size);
			//printf ("name xx-%s sfh %d\n", filename, sfh);
		} else {
			sscanf(p, "name \"%s sfh %d size %x", filename, &sfh, &size);
			//printf ("name %s sfh %d\n", filename, sfh);
			filename[strlen(filename)-1]=0;
		}
		strcpy (name, obj_fh.fh[psfh].path);	
		strcat (name, "/");
		strcat (name, filename);
		add_fh_t (&obj_fh, name, sfh, psfh, size, IS_FILE, NON_EXIST, ACTIVE);
	}
}

void read_fh_map_line_ls (char * buf)
{
	static int sfh = 2;
	int size;
	char name[MAX_NAMELEN];
	char * p = name;
	
	strcpy (name, testdir);
	strcat (name, "/");
	if (strchr(buf, ' ')) {
		sscanf(buf, "%d %s\n", &size, p+strlen(name));
		add_fh_t (&obj_fh, name, sfh, -1, size, IS_FILE, NON_EXIST, ACTIVE);
	} else {
		sscanf(buf, "%s\n", p+strlen(name));
		add_fh_t (&obj_fh, name, sfh, -1, -1, IS_DIR, NON_EXIST, ACTIVE);
	};
	sfh ++;
}

void read_fh_map (char * fh_map_file)
{
	FILE * fp;
	int i = 0;
	char buf[1024];
	int lineno = 0;
	int fh_map_debug =0;
#define FH_MAP_FORMAT_SKIMMER	0
#define FH_MAP_FORMAT_LS		1
	int fh_map_format;

	if (strstr(fh_map_file, ".ski"))
		fh_map_format = FH_MAP_FORMAT_SKIMMER;
	else
		fh_map_format = FH_MAP_FORMAT_LS;

	fp = fopen(fh_map_file, "r");
	if (!fp) {
		printf ("can not opern %s\n", fh_map_file);
		perror("open");
		exit (0);
	}
	RFS_ASSERT (fp!=NULL);
	

	memset(buf, 0, sizeof(buf));

	while (fgets(buf, 1024, fp)) {
		RFS_ASSERT (fh_map_debug==0);
		lineno ++;
		if (rfs_debug)
			printf ("line %d %s", lineno, buf);
		if (lineno % 10000==0)
			printf("%d fh_map entry read\n", lineno);
		if (fh_map_format == FH_MAP_FORMAT_SKIMMER)
			read_fh_map_line_skimmer(buf);
		else 
			read_fh_map_line_ls (buf);
	}
			
	fclose(fp);
	obj_fh.active_fh_max  = obj_fh.fh_max;
	if (fh_map_debug) {
		print_fh_map (&obj_fh);
	}
}

int print_usage()
{
	printf("\n\nagefs fmt4 HOLDER_NUM HOLDER_SIZE DISK_FRAGMENT_SIZE testdir\n");
	printf("Note: fmt4 is used to initialize the holders in a logical partition before starting writing aged files in a specific pattern as by fmt3 command\n");

	printf("\n\nagefs fmt3 aged_file CHUNK_SIZE CHUNK_DISTANCE CHUNK_NUM START_BNO HOLDER_SIZE HOLDER_NUM DISK_FRAGMENT_SIZE testdir\n");
	printf("Note: one file is written as CHUNK_NUM number of continuous chunks on disk, each chunk is of CHUNK_SIZE blocks, the distance between two adjacent chunks is CHUNK_DISTANCE blocks\n");

	printf("\n\nagefs fmt2 size1 .. size_n num testdir\n");
	printf("Note: N file is writen interleavingly for _num_ times, each time _size1_ bytes is written to file1, _size2_ bytes is written to file2, _sizen_ bytes is written to filen\n");

	printf("\n\nagefs EVEN_CHUNK_SIZE FILE_RATIO ACTIVE_RATIO WRITE_CHUNK_NUM MAX_FS_SIZE_MB STAGE_NUM fh_path_map testdir\n");
	printf("Note: EVEN_CHUNK_SIZE: if 1, each file is chopped to equal size chunks, if 0, each file size is chopped randomly but with the average size to be CHUNK_SIZE");
	printf("	  FILE_RATIO: percentage of number of inactive files over number of inactive file system objects\n");
	printf("      ACTIVE_RATIO: percentage of number of active file system objects over all file system objects\n");
	printf("      WRITE_CHUNK_NUM: when a file is initialized, it is written in several open-close session interleved with writes to other files. Except small files where file_size/WRITE_CHUNK_SIZE is less than DISK_FRAGMENT_SIZE, each open-close session on the average write (file_size/WRITE_CHUNK_NUM) bytes. \n");
	printf("	  MAX_FS_SIZE_MB: another condition to stop initialization, either all active file is initialized, or max file system size is reached\n");
	printf("	  STAGE_NUM: divide the writing of files into several stages, each stage finish initialization of some of the files. The bigger the STAGE_NUM, the less concurrency is there\n");
}

inline char * read_line (int disk_index)
{
	static FILE * fp=NULL;
	static int start=0;
	static int start_disk_index=0;
	int i;
	static int finish_flag = 0;

#define READ_LINE_BUF_SIZE 1000
#define READ_LINE_LENGTH 32

	static char line_buf[READ_LINE_BUF_SIZE][READ_LINE_LENGTH];
	start_profile (&read_line_profile);

	if (fp==NULL) {
		if (strcmp(trace_file, "stdin")) {
			fp = fopen(trace_file, "r");
			if (!fp) {
				printf("can not open files %s\n", fp);
				perror("open");
			}
		} else {
			fp = stdin;
		}
		RFS_ASSERT (fp!=NULL);
		for (i=0; i<READ_LINE_BUF_SIZE; i++) {
			start_profile(&fgets_profile);
			if (!fgets(line_buf[i], READ_LINE_LENGTH, fp)) {
				RFS_ASSERT (0);
			}
			end_profile(&fgets_profile);
			//printf ("read_line, line_buf[%d]:%s", i, line_buf[i]);
		}
	}
	
	RFS_ASSERT (disk_index <= start_disk_index+READ_LINE_BUF_SIZE)
	if (disk_index==(start_disk_index+READ_LINE_BUF_SIZE)) {
		if (finish_flag) {
			return NULL;
		}
		start_profile(&fgets_profile);
		if (!fgets(line_buf[start], READ_LINE_LENGTH, fp)) {
			end_profile(&fgets_profile);
			fclose(fp);
			finish_flag = 1;
			return NULL;
		}
		end_profile(&fgets_profile);
		//printf ("read_line, line_buf[%d]:%s", start, line_buf[start]);
		start = (start+1) % READ_LINE_BUF_SIZE;
		start_disk_index ++;
	}
	RFS_ASSERT (disk_index < start_disk_index+READ_LINE_BUF_SIZE)
	i = (start+disk_index-start_disk_index)%READ_LINE_BUF_SIZE;

	end_profile (&read_line_profile);
	return (line_buf[i]);
}

int print_result()
{
	struct statfs stfs;
	int ret;
	static struct statfs first_stfs;
	static int first_entry = 1;

	ret = statfs (testdir, &stfs);
	RFS_ASSERT (ret == 0);
	if (first_entry) {
		first_entry = 0;
		first_stfs = stfs;
	}

	fprintf(stderr, "active_file_num %d active_dir_num %d age_file_num %d age_dir_num %d\n",
		active_file_num, active_dir_num, age_file_num, age_dir_num);
	fprintf(stderr, "number of used file nodes %d, used (4K) blocks in fs %d (%d MB)\n", first_stfs.f_ffree-stfs.f_ffree, first_stfs.f_bfree - stfs.f_bfree, (first_stfs.f_bfree-stfs.f_bfree)/(1000000/DISK_FRAGMENT_SIZE));
	fprintf(stderr, "assure_create_num %d assure_mkdir_num %d\n", assure_create_num, assure_mkdir_num);
}

typedef struct {
    int     pcnt;       /* percentile */
    int     size;       /* file size in KB */
} sfs_io_file_size_dist;

sfs_io_file_size_dist Default_file_size_dist[] = {
    /* percentage   KB size */
#ifdef notdef
	{   100,    128},			
    {    94,     64},           /*  4% */
    {    97,    128},           /*  3% */
#endif
    {    33,      1},           /* 33% */
    {    54,      2},           /* 21% */
    {    67,      4},           /* 13% */
    {    77,      8},           /* 10% */
    {    85,     16},           /*  8% */
    {    90,     32},           /*  5% */
    {    94,     64},           /*  4% */
    {    97,    128},           /*  3% */
    {    99,    256},           /*  2% */
    {   100,   1024},           /*  1% */
    {     0,      0}
};

/*
 * For a value between 0-99, return a size based on distribution
 */
static int
get_file_size()
{
	static file_array_initialized = 0;
	static int file_size_array[100];
	int i;

	i = random() % 100;

    if (i < 0 || i > 99)
    return (0);

    if (file_array_initialized == 0) {
	    int j, k;
    	for (j = 0, k = 0; j < 100; j++) {
        	if (j >= Default_file_size_dist[k].pcnt &&
            	Default_file_size_dist[k + 1].size != 0)
    	    k++;
        	file_size_array[j] = Default_file_size_dist[k].size * 1024;
    	}
    	file_array_initialized++;
    }
    return (file_size_array[i]);
}

int range_random(int min, int max)
{
	int i;
	i = (random()%(max-min)) + min;
	return i;
}

/* answer 1 with a probability of percent/100 */
int decide(int percent)
{
	int i = random()%100;
	if (i<percent)
		return 1;
	else
		return 0;
}

int select_obj (fh_info_t * fhp, int ftype, int exist_flag, int active_flag, int min, int max)
{
	int i;
	int sfh, count = 0;

	//printf ("select_obj %s %s %s\n", ftypename[ftype], existname[exist_flag], activename[active_flag]);
	if (active_flag == ACTIVE) {
		sfh = range_random (0, fhp->active_fh_max);
		for (i=0; i<fhp->active_fh_max; i++) {
			if ((fhp->fh[sfh].flag == FH_T_FLAG_IN_USE) &&
				((ftype==DONT_CARE) || (ftype ==fhp->fh[sfh].ftype)) &&
				((exist_flag==DONT_CARE) || (fhp->fh[sfh].exist_flag == exist_flag))) {
				return sfh;
				}
			sfh = (sfh+1) % fhp->active_fh_max;
		}
	} else {
		//min = 0;
		//max = fhp->fh_max;
		//printf ("select_obj min %d max %d\n", min, max);
		RFS_ASSERT (active_flag == DONT_CARE);
		RFS_ASSERT (exist_flag == EXIST);
		sfh = range_random (min, max);
		for (i=min; i<max; i++) {
		//	printf("check %d\n", sfh);
			RFS_ASSERT ((sfh>=min) && (sfh<max));
			if ((fhp->fh[sfh].flag == FH_T_FLAG_IN_USE) &&
			    ((ftype==DONT_CARE) || (fhp->fh[sfh].ftype == ftype)) &&
				(fhp->fh[sfh].exist_flag == EXIST)) {
				return sfh;
			}
			sfh++;
			if (sfh==max)
				sfh = min;
		}
	}
/*
	for (i=min; i<max; i++) {
		if ((fhp->fh[i].flag == FH_T_FLAG_IN_USE) &&
			(fhp->fh[i].ftype == IS_FILE)				) {
			if (fhp->fh[i].exist_flag == EXIST) {
				printf ("actually %d\n", i);
			}
			RFS_ASSERT (fhp->fh[i].exist_flag != EXIST);
			RFS_ASSERT (fhp->fh[i].exist_flag == COMPLETE);
		}
	}
*/
	return -1;
}

/* append "size" to file "path" */
int append_file (int sfh, char * path, int size)
{
	int fd;
	int written_bytes = 0;
	int ret;
#define BUF_SIZE 32768
	static char buf[BUF_SIZE];

	if (rfs_debug)
		printf ("sfh %d append_file %s size %d\n", sfh, path, size);

	fd = open (path, O_WRONLY|O_APPEND);
	if (fd==-1)
		perror(path);
	RFS_ASSERT (fd > 0);
	
	while (written_bytes+NFS_MAXDATA < size) {
		ret = loop_write (fd, buf, NFS_MAXDATA);
		RFS_ASSERT (ret!=-1);
		written_bytes += NFS_MAXDATA;
	}
	ret = loop_write (fd, buf, size-written_bytes);
	RFS_ASSERT (ret!=-1);
	close(fd);
}

int get_write_size (int target_size, int cur_size)
{
	int i;

	if (target_size - cur_size < MIN_WRITE_SIZE)
		return (target_size - cur_size);

	/* target_size/WRITE_CHUNK_NUM would be the average value of i */
	if (target_size < WRITE_CHUNK_NUM) {
		i = MIN_WRITE_SIZE;
	} else {

		if (EVEN_CHUNK_SIZE)
			i = target_size/WRITE_CHUNK_NUM;
		else
			i = random() % (2*(target_size/WRITE_CHUNK_NUM));

		if (i < MIN_WRITE_SIZE)
			i = MIN_WRITE_SIZE;
	}

	if (i > (target_size - cur_size))
		i = target_size - cur_size;

	return i;
}

FILE * fplog;


int CHUNK_SIZE;
int CHUNK_DISTANCE;
int CHUNK_NUM;
int START_BNO;
int HOLDER_NUM;
int HOLDER_SIZE;
int INDIRECT_FANOUT;
char agename[1024];

#define MAX_DISK_FRAGMENT_SIZE 4096
#define MAX_HOLDER_SIZE 10
#define HOLDER_DIR_NUM 1
#define DUMMY_FILE_COUNT 1000

main4(int argc, char ** argv)
{
	int i, j;
	char name[256];
	char cmd[1024];
	char * buf;
	int fd, ret;
	char testdir[1024];

	if (argc!=6) {
		print_usage();
		return;
	}
	i = 2;
	HOLDER_NUM = atoi(argv[i++]);
	HOLDER_SIZE = atoi(argv[i++]);
	DISK_FRAGMENT_SIZE = atoi(argv[i++]);
	RFS_ASSERT (DISK_FRAGMENT_SIZE <= MAX_DISK_FRAGMENT_SIZE);
	DISK_FRAGMENT_SIZE_MASK = ~(DISK_FRAGMENT_SIZE-1);
	RFS_ASSERT ((DISK_FRAGMENT_SIZE_MASK == 0xFFFFF000) ||
				(DISK_FRAGMENT_SIZE_MASK == 0xFFFFFC00)		);
	strcpy (testdir, argv[i]);

	fprintf(fplog, "main4: initialize the holders HOLDER_NUM %d HOLDER_SIZE %d DISK_FRAGMENT_SIZE %d testdir %s\n",
			HOLDER_NUM, HOLDER_SIZE, DISK_FRAGMENT_SIZE, testdir);
	fflush(fplog);

	buf = (char *)malloc (HOLDER_SIZE*DISK_FRAGMENT_SIZE);
	RFS_ASSERT (buf);

	/* create some dummy files */
	for (i=0; i<DUMMY_FILE_COUNT; i++) {
		memset (name, 0, sizeof(name));
		sprintf(name, "%s/dummy%d", testdir, i);
 		fd = creat (name, S_IRWXU);
		if (fd == -1) {
			perror(name);
			exit(-1);
		}
		close (fd);
	}

	/* create directories */
	if (HOLDER_DIR_NUM !=1) {
		for (i=0; i<HOLDER_DIR_NUM; i++) {
			memset (name, 0, sizeof(name));
			sprintf(name, "%s/%d", testdir, i);
 			ret = mkdir (name, S_IRWXU);
			if (ret == -1) {
				perror(name);
				exit(-1);
			}
		}
	}

	/* create regular files */
	for (j=0; j<HOLDER_DIR_NUM; j++) {
		for (i=0; i<HOLDER_NUM/HOLDER_DIR_NUM; i++) {
			if (HOLDER_DIR_NUM == 1)  
				sprintf(name, "%s/%d", testdir, i);
			else
				sprintf(name, "%s/%d/%d", testdir, j, i);
			fd = open (name, O_CREAT|O_WRONLY);
			if (fd == -1) {
				perror(name);
				exit(-1);
			}
			ret = loop_write (fd, buf, HOLDER_SIZE*DISK_FRAGMENT_SIZE);
			close (fd);
			if (ret == -1) 
				break;
		}
	}

#ifdef notdef
	/* delete the dummy files */
	for (i=0; i<DUMMY_FILE_COUNT; i++) {
		memset (name, 0, sizeof(name));
		sprintf(name, "%s/dummy%d", testdir, i);
 		ret = unlink (name);
		if (ret == -1) {
			perror(name);
			exit(-1);
		}
	}
#endif
}

int append_space_occupier()
{
	static char name [1024];
	static FILE * fp = NULL;
	char buf[MAX_DISK_FRAGMENT_SIZE];
	int ret;
	if (fp == NULL) {
		sprintf(name, "%s/space_ocuupier", testdir);
		fp = fopen (name, "a+");
		RFS_ASSERT (fp!= NULL);
		ret = fwrite (buf, DISK_FRAGMENT_SIZE, 1, fp);
		if (ret != 1) {
			perror("append space occupier");
		}
		fclose (fp);
		fp = NULL;
	};
	ret = fwrite (buf, DISK_FRAGMENT_SIZE, 1, fp);
	if (ret != 1) {
		perror("append space occupier");
	}
	RFS_ASSERT (ret == 1);
}

int create_one_dummy_file()
{
	int i, fd, ret;
	static int index = 0;
	static char name[1024];
	struct stat st;

	for (i=0; i<DUMMY_FILE_COUNT; i++) {
		index = (index+1) % DUMMY_FILE_COUNT;
		sprintf(name, "%s/dummy%d", testdir, i);
		ret = stat (name, &st);
		if (ret == -1) {
			RFS_ASSERT (errno == ENOENT);
			fd = open (name, O_CREAT|O_WRONLY);
			RFS_ASSERT (fd >=0);
			close (fd);
			return 0;
		};
	}
}

int delete_one_dummy_file()
{
	int i,ret;
	static int index = 0;
	static char name[1024];
	struct stat st;

	for (i=0; i<DUMMY_FILE_COUNT; i++) {
		index = (index+1) % DUMMY_FILE_COUNT;
		sprintf(name, "%s/dummy%d", testdir, i);
		ret = stat (name, &st);
		if (ret == 0) {
 			ret = unlink (name);
			RFS_ASSERT (ret == 0);
			return;
		} else
		 	RFS_ASSERT (errno == ENOENT);
	}
	RFS_ASSERT (0);
}

int create_sub_holder_file(int holderno, int sub_holderno)
{
	char name[256];
	int fd, ret;
	static char buf[MAX_DISK_FRAGMENT_SIZE];


	RFS_ASSERT (MAX_DISK_FRAGMENT_SIZE >= DISK_FRAGMENT_SIZE);

	sprintf(name, "%d.%d", holderno, sub_holderno);
	printf ("create/write %s\n", name);
	fd = open (name, O_CREAT|O_WRONLY);
	RFS_ASSERT (fd >=0);
	loop_write (fd, buf, DISK_FRAGMENT_SIZE);
	close(fd);
}

int get_free_blocks ()
{
	static struct statfs stfs;
	int ret;

	ret = statfs (testdir, &stfs);
	RFS_ASSERT (ret == 0);
	return (stfs.f_bfree);
}

int print_free_blocks (char *string)
{
	static struct statfs stfs;
	int ret;

	ret = statfs (testdir, &stfs);
	RFS_ASSERT (ret == 0);
	printf("%s f_bfree %d \n", string, stfs.f_bfree);
}

int check_free_blocks (int num)
{
	static struct statfs stfs;
	int ret;

	ret = statfs (testdir, &stfs);
	RFS_ASSERT (ret == 0);
	if (stfs.f_bfree!=num) {
		printf("f_bfree %d expected %d\n", stfs.f_bfree, num);
		RFS_ASSERT (0);
	}
}

int progress_on_aged_file(int * num)
{
	char buf[MAX_DISK_FRAGMENT_SIZE*MAX_HOLDER_SIZE];
	static need_indirect_blocks = 0;
	static skip_for_indirect_blocks = 0;
	static int blkno = 0;

	printf ("\n");
	print_free_blocks("progress_on_aged_file begin");

	if (skip_for_indirect_blocks == need_indirect_blocks) {
		//check_free_blocks(free_block_num);
		//RFS_ASSERT (free_block_num >= (1+need_indirect_blocks));
		(*num) --;
		printf("append to aged file %d bytes\n", DISK_FRAGMENT_SIZE);
		append_file (0, agename, DISK_FRAGMENT_SIZE);
		//*free_block_num -= (need_indirect_blocks +1)
		//check_free_blocks(free_block_num);

		blkno ++;
		if (((blkno - 12) % INDIRECT_FANOUT) == 0) {
			if (((blkno - (INDIRECT_FANOUT+12)) % (INDIRECT_FANOUT*INDIRECT_FANOUT)) == 0) {
				if (blkno == 12 + INDIRECT_FANOUT + INDIRECT_FANOUT*INDIRECT_FANOUT) {
					printf ("need_indirect_blocks is set to 3 blkno %d\n", blkno);
					need_indirect_blocks = 3;
				} else {
					printf ("need_indirect_blocks is set to 2 blkno %d\n", blkno);
					need_indirect_blocks = 2;
				}
			} else {
				printf ("need_indirect_blocks is set to 1 blkno %d\n", blkno);
				need_indirect_blocks = 1;
			};
		} else {
			need_indirect_blocks = 0;
		}
		skip_for_indirect_blocks = 0;  
	} else {
		skip_for_indirect_blocks ++;  
	}

	printf ("skip_for_indirect_blocks -- %d\n", skip_for_indirect_blocks);
	print_free_blocks("progress_on_aged_file end");
}

int free_blocks (char * agename, int start, int num)
{
	int holderno;
	char name [128];
	int ret;
	struct stat st;
	int sub_holderno;
	int i;

	printf ("free_blocks start %d  num %d\n", start, num);

BEGIN:
	check_free_blocks(0);
	if (num == 0)
		return start;
   	holderno = start/HOLDER_SIZE;
	sub_holderno = start%HOLDER_SIZE;

	sprintf (name, "%d", holderno);
	
	ret = stat (name, &st);
	if (ret == -1) {
		RFS_ASSERT (errno == ENOENT);
		for (i=sub_holderno; (i<HOLDER_SIZE && num>0); i++) {
			sprintf(name, "%d.%d", holderno, i);
			ret = stat (name, &st);
			if (ret == 0) {

				printf ("sub_holder file %s is unlinked\n", name);
				ret = unlink(name);
				RFS_ASSERT (ret == 0);

				create_one_dummy_file();

				printf ("write to age file %d bytes\n", DISK_FRAGMENT_SIZE);

				progress_on_aged_file(&num);

			} else { 
				printf ("sub_holder file %s is already used\n", name);
				RFS_ASSERT ((ret == -1) && (errno ==ENOENT));
			}
			start ++;
		}
		goto BEGIN; 
	}
	
	RFS_ASSERT (ret == 0);
	RFS_ASSERT (st.st_size == HOLDER_SIZE * DISK_FRAGMENT_SIZE);
	printf ("holder file %s is unlinked\n", name);
	ret = unlink(name);
	RFS_ASSERT (ret == 0);
	check_free_blocks(HOLDER_SIZE);

	/* create the sub holders before the first slot that we need */
	for (i=0; i<sub_holderno; i++) {
		delete_one_dummy_file();
		create_sub_holder_file (holderno, i);
	}

	/*
	i = (HOLDER_SIZE - sub_holderno) < num? (HOLDER_SIZE - sub_holderno): num;
	sub_holderno += i;
	start += i;
	num -= i;
	check_free_blocks (i);
	ret = loop_write (agefd, buf, DISK_FRAGMENT_SIZE*i);
	RFS_ASSERT (ret != -1);
	*/
	i = HOLDER_SIZE - sub_holderno;
	check_free_blocks(i);

	while ((sub_holderno < HOLDER_SIZE) && (num>0)) {
		sub_holderno ++;
		start ++;

		progress_on_aged_file(&num);
						
		RFS_ASSERT (ret != -1);
	}

	/* create the sub holders after the slot that we need */
	for (i=sub_holderno; i<HOLDER_SIZE; i++) {
		delete_one_dummy_file();
		create_sub_holder_file (holderno, i);
	}

	create_one_dummy_file();
	goto BEGIN;
}

int main3(int argc, char ** argv)
{

	int block_anchor;
	int i;
	int agefd;
	char * buf;
	char cwd[1024];
	char * ret;
	struct stat st;

	if (argc!=11) {
		print_usage();
		return;
	}
	i = 2;

	CHUNK_SIZE = atoi(argv[i++]);
	CHUNK_DISTANCE = atoi (argv[i++]);
	CHUNK_NUM = atoi (argv[i++]);
	START_BNO = atoi (argv[i++]);
	HOLDER_SIZE = atoi (argv[i++]);
	RFS_ASSERT (HOLDER_SIZE <= MAX_HOLDER_SIZE);
	HOLDER_NUM = atoi (argv[i++]);
	DISK_FRAGMENT_SIZE = atoi (argv[i++]);
	RFS_ASSERT (DISK_FRAGMENT_SIZE <= MAX_DISK_FRAGMENT_SIZE);
	INDIRECT_FANOUT = (DISK_FRAGMENT_SIZE/sizeof(int));
	strcpy (testdir, argv[i++]);
	strcpy (agename, testdir);
	strcat (agename, argv[i++]);
	ret = stat (agename, &st);
	if (ret!=-1) {
		printf ("%s already exists\n", agename);
	}	
	RFS_ASSERT (errno == ENOENT);

	fprintf(fplog, "main3: to age one file %s in a customized way, CHUNK_SIZE %d CHUNK_DISTANCE %d CHUNK_NUM %d START_BNO %d HOLDER_SIZE %d HOLDER_NUM %d DISK_FRAGMENT_SIZE %d MAX_DISK_FRAGMENT_SIZE %d testdir %s\n", agename, CHUNK_SIZE, CHUNK_DISTANCE, CHUNK_NUM, START_BNO, HOLDER_SIZE, HOLDER_NUM, DISK_FRAGMENT_SIZE, MAX_DISK_FRAGMENT_SIZE, testdir);
	fflush(fplog);

	/* change working directory */
	ret = getcwd(cwd, sizeof(cwd));
	RFS_ASSERT (ret == cwd);
	i = chdir (testdir);
	RFS_ASSERT (i==0);

	if (START_BNO == -1) {
		block_anchor = random() % (HOLDER_NUM*HOLDER_SIZE);
	} else {
		RFS_ASSERT (START_BNO >=0);
		block_anchor = START_BNO % (HOLDER_NUM*HOLDER_SIZE);
	}
	while (get_free_blocks()!=0) {
		print_free_blocks("fill up initial file system blocks using space occupier");
		append_file (0, "/b6/space_occupier", DISK_FRAGMENT_SIZE);
	};
	delete_one_dummy_file();
	agefd = open (agename, O_CREAT|O_WRONLY);
	RFS_ASSERT (agefd>=0);
	close (agefd);

	buf = (char *)malloc (CHUNK_SIZE*DISK_FRAGMENT_SIZE);
	RFS_ASSERT (buf);

	for (i=0; i<CHUNK_NUM; i++) {
		block_anchor = (block_anchor + CHUNK_DISTANCE) % (HOLDER_NUM * HOLDER_SIZE);	
		block_anchor = free_blocks (agename, block_anchor, CHUNK_SIZE);
	}

	check_free_blocks(0);
	i = chdir (cwd);
	RFS_ASSERT (i==0);
}

int main2(int argc, char ** argv)
{
	int i, j;
	int size[3], num;
	FILE * fp[3];
	char name[3][128];
	char cmd[1024];

	if (argc <= 4) {
		print_usage();
		return;
	}
	num = atoi(argv[argc-2]);
	strcpy (testdir, argv[argc-1]);
	fprintf(fplog, "main2: generate interleaved files\n");
    fprintf(fplog, "testdir %s number of files %d ", testdir, num);
	for (i=0; i<argc-4; i++) {
		size[i] = atoi(argv[i+2]);
		fprintf(fplog, "size[%d] %d ", i, size[i]);

		RFS_ASSERT (size[i] >=0 && size[i] < 1000000000);
		strcpy (name[i], testdir);
		sprintf (name[i], "%s/file%d", testdir, i);
		sprintf(cmd, "touch %s", name[i]);
		system(cmd);
		printf ("write %s \n", name[i]);
	};
	fprintf(fplog, "\n");
	fflush(fplog);

	for (j=0; j<num; j++) {
		for (i=0; i<argc-4; i++)
			append_file (i, name[i], size[i]);
	}
}

int main(int argc, char ** argv)
{
	int i;
	char cmd[1024];
	char AGELOG_NAME[1024]= "/home/ningning/agefs.log";

	sprintf(cmd, "date >> %s", AGELOG_NAME);
	system (cmd);
	fplog = fopen(AGELOG_NAME, "a");
	RFS_ASSERT (fplog);
	for (i=0; i<argc; i++) {
		fprintf(fplog, "%s ", argv[i]);
	}
	fprintf (fplog, "\n");

	if (argc>1 && (!strcmp(argv[1], "fmt2"))) 
		main2 (argc, argv);
	else if (argc>1 && (!strcmp(argv[1], "fmt3"))) 
		main3 (argc, argv);
	else if (argc>1 && (!strcmp(argv[1], "fmt4"))) 
		main4 (argc, argv);
	else 
		main1 (argc, argv);
END:
	fclose (fplog);
	sprintf(cmd, "date >> %s", AGELOG_NAME);
	system (cmd);
}

int main1(int argc, char ** argv)
{
	char * buf;
	static int disk_index=0;
	int nfs3proc, size, off, count;
	char procname[16];
	struct stat st;
	int ret;
	int i,j,k;
	int ftype_flag = 0, active_flag = 0;
	char name[MAX_PLAY_PATH_SIZE];	
	int sfh, psfh;
	char mapname[1024];

	profile_t create_profile, write_profile;
	if (argc!=9) {
		print_usage();
		return;
	}

	init();
	EVEN_CHUNK_SIZE = atoi(argv[1]);
	RFS_ASSERT ((EVEN_CHUNK_SIZE==0) || (EVEN_CHUNK_SIZE==1));
	FILE_RATIO = atoi (argv[2]);
	ACTIVE_RATIO = atoi(argv[3]);
	WRITE_CHUNK_NUM = atoi(argv[4]);
	MAX_FS_SIZE_MB = atoi(argv[5]);

	if (WRITE_CHUNK_NUM==0)
		MIN_WRITE_SIZE = 2000000000;
	else {
		//MIN_WRITE_SIZE = DISK_FRAGMENT_SIZE;
		MIN_WRITE_SIZE = 1;
	}

	STAGE_NUM = atoi (argv[6]);
	strcpy (mapname, argv[7]);
	strcpy (testdir, argv[8]);
	ret = stat (testdir, &st);
	if ((ret == -1) && (errno==ENOENT)) {
		ret = mkdir (testdir, S_IRWXU);
	}
	RFS_ASSERT (ret >= 0);

	
	/* add testdir to obj_fh */
	add_fh_t (&obj_fh, testdir, 1, -1, -1, IS_DIR, EXIST, ACTIVE);
	exist_active_obj_num ++;
	if (ACTIVE_RATIO >0) 
		read_fh_map (mapname);

	print_fh_map(&obj_fh);
	init_profile_variables();

	fprintf(fplog, "main1: populate the file system with both trace files and randomly generated files\n");
	fprintf(fplog, "EVEN_CHUNK_SIZE %d FILE_RATIO %d ACTIVE_RATIO %d WRITE_CHUNK_NUM %d MAX_FS_SIZE_MB %d STAGE_NUM %d fh_map %s testdir %s\n", EVEN_CHUNK_SIZE, FILE_RATIO, ACTIVE_RATIO, WRITE_CHUNK_NUM, MAX_FS_SIZE_MB, STAGE_NUM, mapname, testdir);
	system("date");
	printf("EVEN_CHUNK_SIZE %d FILE_RATIO %d ACTIVE_RATIO %d WRITE_CHUNK_NUM %d MAX_FS_SIZE_MB %d STAGE_NUM %d fh_map %s testdir %s\n", EVEN_CHUNK_SIZE, FILE_RATIO, ACTIVE_RATIO, WRITE_CHUNK_NUM, MAX_FS_SIZE_MB, STAGE_NUM, mapname, testdir);
	fflush(fplog);

	profile_fp = fplog;
	init_profile ("create_profile", &create_profile);
	init_profile ("write_profile", &write_profile);

	start_profile (&create_profile);
	printf ("start creat/mkdir, active_obj_num %d\n", active_obj_num);
	for (i=0; (exist_active_obj_num <= active_obj_num) && (fs_size_MB < MAX_FS_SIZE_MB); i++) {

		if ((i!=0) && ((i%10000)==0)) {
			fprintf (stderr, "\n%d object created, exist_active_obj_num %d expected size %d MB\n", i, exist_active_obj_num, fs_size_MB);
		}

		/* decide on the exact active obj or populated obj */
		if (decide(ACTIVE_RATIO)) {
			sfh = select_obj (&obj_fh, DONT_CARE, NON_EXIST, ACTIVE, 0, obj_fh.fh_max);
			if (sfh == -1)
				break;

			obj_fh.fh[sfh].exist_flag = EXIST;
			exist_active_obj_num ++;
			ftype_flag = obj_fh.fh[sfh].ftype;
			size = obj_fh.fh[sfh].size;
		} else {
			psfh = select_obj (&obj_fh, IS_DIR, EXIST, DONT_CARE, 0, obj_fh.fh_max);
			strcpy (name, obj_fh.fh[psfh].path);
			sfh = obj_fh.fh_max;
			sprintf(name+strlen(name), "/AGE%d", obj_fh.fh_max); 

			/* decide next obj is file or directory */
			if (decide(FILE_RATIO)) {
				ftype_flag = IS_FILE;
				size = get_file_size();
			} else {
				ftype_flag = IS_DIR;
				size = -1;
			}
			add_fh_t (&obj_fh, name, sfh, psfh, size, ftype_flag, EXIST, INACTIVE);

		}

		/* make sure/create the  obj pathname on disk */
		assure_exist (sfh, obj_fh.fh[sfh].path, ftype_flag);
		/* write file to sizes according certain distribution 
		if (ftype_flag == IS_FILE) 
			append_file (obj_fh.fh[sfh].path, obj_fh.fh[sfh].size);
		*/
	}
	fprintf (stderr, "\n%d object created, exist_active_obj_num %d expected size %d MB\n", i, exist_active_obj_num, fs_size_MB);

	end_profile (&create_profile);
	start_profile (&write_profile);
	//write_file_range (0, obj_fh.fh_max);
	for (i=1; i<=STAGE_NUM; i++) {
		write_file_range (obj_fh.fh_max*(i-1)/STAGE_NUM, obj_fh.fh_max*i/STAGE_NUM);
		//fprintf(stderr, "getchar\n");
		//getchar();
	}
	end_profile (&write_profile);

	print_profile ("create_profile", &create_profile);
	print_profile ("write_profile", &write_profile);
	printf ("exist_active_obj_num %d active_obj_num %d fs_size_MB %d\n",
			exist_active_obj_num, active_obj_num, fs_size_MB);

	print_fh_map(&obj_fh);
	print_result();
	printf ("end of file system initialization\n");
	system("date");
}

int write_file_range (int min, int max)
{
	int i, j, k, sfh;
	int write_size, disk_write_size;

	i = 0, j=0, k=0;
	printf ("start writing files min %d max %d\n", min, max);
	while (1) {
		sfh = select_obj (&obj_fh, IS_FILE, EXIST, DONT_CARE, min, max);
/*
		if (!decide(obj_fh.fh[sfh].size*100/(MIN_WRITE_SIZE*WRITE_CHUNK_NUM))) {
			printf("skip writing small file\n");
			continue;
		}
*/
		if (sfh == -1)
			break;
		write_size = get_write_size (obj_fh.fh[sfh].size, obj_fh.fh[sfh].cur_size);
		obj_fh.fh[sfh].cur_size += write_size;
		if (obj_fh.fh[sfh].cur_size == obj_fh.fh[sfh].size) {
			obj_fh.fh[sfh].exist_flag = COMPLETE;
		};

#define ACCUMULATE_SMALL_WRITE	
// This option improves speed by 12 times.
#ifdef ACCUMULATE_SMALL_WRITE
		obj_fh.fh[sfh].accumulated_write_size += write_size;
		if (obj_fh.fh[sfh].exist_flag == COMPLETE) {
			disk_write_size = obj_fh.fh[sfh].accumulated_write_size;
		} else {
			disk_write_size = (obj_fh.fh[sfh].accumulated_write_size -
							  (obj_fh.fh[sfh].accumulated_write_size%DISK_FRAGMENT_SIZE));
		};
		obj_fh.fh[sfh].accumulated_write_size -= disk_write_size;
#else
		disk_write_size = write_size;
#endif

		if (disk_write_size >0) {
			append_file (sfh, obj_fh.fh[sfh].path, disk_write_size);
			if ((i%1000)==0) {
				printf ("%d C ", i);
				fflush(stdout);
				k++;
				if ((k%10)==0)
					printf("\n");
			}
			i++;
		} else {
			if ((j%100000)==0) {
				printf ("%d c ", j);
				fflush(stdout);
				k++;
				if ((k%10)==0)
					printf("\n");
			}
			j++;
		}
	}
}

