#include "mount_prot.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int null_int;
static void *null_result = (void *)&null_int;

void *
mountproc3_null_3_svc(void *argp, struct svc_req *rqstp)
{
    return null_result;
}

mountres3 *
mountproc3_mnt_3_svc(dirpath *argp, struct svc_req *rqstp)
{
    static char fhbytes[] = {0, 0, 0, 0, 0, 0, 0, 1};
    static fhandle3 fh = {8, fhbytes};
    static mountres3 result;
    static int auth_flavors = AUTH_UNIX;

    result.fhs_status = MNT3_OK;
    result.mountres3_u.mountinfo.fhandle = fh;
    result.mountres3_u.mountinfo.auth_flavors.auth_flavors_len = 1;
    result.mountres3_u.mountinfo.auth_flavors.auth_flavors_val = &auth_flavors;

    return &result;
}

mountlist *
mountproc3_dump_3_svc(void *argp, struct svc_req *rqstp)
{
    return null_result;
}

void *
mountproc3_umnt_3_svc(dirpath *argp, struct svc_req *rqstp)
{
    return null_result;
}

void *
mountproc3_umntall_3_svc(void *argp, struct svc_req *rqstp)
{
    return null_result;
}

exports *
mountproc3_export_3_svc(void *argp, struct svc_req *rqstp)
{
    return null_result;
}
