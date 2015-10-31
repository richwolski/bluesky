#ifdef notdef
int create_mkdir_op (int flag)
{
	static int fhno = 0;
	char name[MAX_NAMELEN];
	char command[MAX_COMMAND_LEN];
	int i;
	int fd;
	int count = 0;
	fh_info_t * fh_infop;

	while (count++ < 100) {
		i = random()%dir_fh.fh_max;
		if (dir_fh.fh[i].flag==FH_T_FLAG_IN_USE) {
			assure_exist(dir_fh.fh[i].path);
			strcpy (name, dir_fh.fh[i].path);
			if (flag == IS_FILE) {
				sprintf (name+strlen(name), "AGEfile%d", fhno++);
				fd = creat (name, S_IRWXU);
				age_create_num++;
				//printf ("create fd %d\n", fd);
				close(fd);
				fh_infop = &file_fh;
			} else {
				sprintf (name+strlen(name), "AGEdir%d", fhno++);
				fd = mkdir (name, S_IRWXU);
				age_mkdir_num++;
				fh_infop = &dir_fh;
			}
			if (fd == -1) {
				perror("");
				if (errno == ENOENT) {
					dir_fh.fh[i].flag = FH_T_FLAG_FREE;
					continue;
				} else
					RFS_ASSERT (0);
			}
			add_fh_t (fh_infop, name, EXIST);
			RFS_ASSERT (fd >=0);
			return 0;
		}
	};
	return -1;
}

int remove_op ()
{
	int i;
	int count = 0;
	int ret;

	while (count++<100) {
		i = random()%file_fh.fh_max;
		if (file_fh.fh[i].flag == FH_T_FLAG_IN_USE) {
/*
			if (!strstr(file_fh.fh[i].path, "AGE"))
				continue;
*/
			assure_exist(file_fh.fh[i].path);
			ret = remove (file_fh.fh[i].path);
			RFS_ASSERT (ret ==0);
			file_fh.fh[i].flag = FH_T_FLAG_FREE;
			return 0;
		}
	}
	return -1;
}

int rmdir_op()
{
	int i;
	int count=0;
	char command[MAX_COMMAND_LEN];
	int ret;

	while (count++<100) {
		i = random()%dir_fh.fh_max;
		if ( (dir_fh.fh[i].flag == FH_T_FLAG_IN_USE) ) {
/*
			if (!strstr(file_fh.fh[i].path, "AGE"))
				continue;
*/
			assure_exist(file_fh.fh[i].path);
			ret = rmdir (dir_fh.fh[i].path);
			if (ret == 0) {
				dir_fh.fh[i].flag = FH_T_FLAG_FREE;
				return 0;
			}
			RFS_ASSERT ((ret == -1) && (errno == ENOTEMPTY));
			//strcpy (command, "rm -r %s", dir_fh.fh[i].path);
			//system (command);
		}
	}
	return -1;
}

int write_op (int off, int size)
{
	static char buf[NFS_MAXDATA];
	int i;
	int count=0;
	int fd;
	int ret;
	struct stat st;

	RFS_ASSERT (size <= NFS_MAXDATA);
	while (count++<100) {
		i = random()%file_fh.fh_max;
		if ( (file_fh.fh[i].flag == FH_T_FLAG_IN_USE) ) {
			assure_exist(file_fh.fh[i].path);
			fd = open(file_fh.fh[i].path, O_WRONLY);
			if (fd == -1)
				perror("");
			//else 
				//printf ("write fd %d\n", fd);
			RFS_ASSERT (fd!=-1);
			fstat (fd, &st);
			if (st.st_size < (off+size)) {
				int written_bytes = 0;
				while (written_bytes+NFS_MAXDATA < off+size-st.st_size) {
					loop_write (fd, buf, NFS_MAXDATA);
					written_bytes += NFS_MAXDATA;
				}
				loop_write (fd, buf, off+size-st.st_size-written_bytes);
				if (strstr(file_fh.fh[i].path, "AGE")) {
					age_write_num+=(written_bytes+NFS_MAXDATA-1)/NFS_MAXDATA;
				} else 
					nonage_write_num+=(written_bytes+NFS_MAXDATA-1)/NFS_MAXDATA;
			} else
				overlap_write_num++;
/*
			if (strstr(file_fh.fh[i].path, "AGE")) {
				age_write_num++;
			} else 
				nonage_write_num++;
			loop_write (fd, buf, size);
*/
			close(fd);
			return 0;
		};
	}
	return -1;
}

int truncate_op(int size)
{
	int i;
	int count=0;
	int ret;

	while (count++<100) {
		i = random()%file_fh.fh_max;
		if ( (file_fh.fh[i].flag == FH_T_FLAG_IN_USE) ) {
/*
			if (!strstr(file_fh.fh[i].path, "AGE"))
				continue;
*/
			assure_exist (file_fh.fh[i].path);
			ret = truncate(file_fh.fh[i].path, size);
			if (ret ==0) 
				return 0;
			RFS_ASSERT (errno == ENOENT);
			file_fh.fh[i].flag = FH_T_FLAG_FREE;
			continue;	
		}
	};
	return -1;
}

int aging_files ()
{
	char name[MAX_NAMELEN];
	int sfh;	/* file to be aged */
	int psfh;
	int ret;
	struct stat st;
	int agefd;

	/* get the sfh and size of the selected file to be aged */
	sfh = select_obj (&obj_fh, IS_FILE, EXIST, ACTIVE);
	ret = stat (obj_fh.fh[sfh].path, &st);
	RFS_ASSERT (ret == 0);
	ret = truncate(obj_fh.fh[i].path, st.st_size/2);
	RFS_ASSERT (ret==0);

	psfh = obj_fh.fh[sfh].psfh;
	strcpy (name, obj_fh.fh[psfh].path);
	sprintf(name+strlen(name), "/AGE%d", obj_fh.fh_max); 
	agefd = creat (name, S_IRWXU);
	//write (agefs, buf, 0);

	add_fh_t (&obj_fh, name, sfh, psfh, size, ftype_flag, EXIST, INACTIVE);
}
#endif
