/*
--------mod_nwayacd
--------database.c
--------座席排队数据表
--------许可协议：Apache-2.0
--------开发团队：上海宁卫信息技术有限公司
--------联系Email:lihao@nway.com.cn
--------开始开发时间：2020-8-1 中华人民共和国建军节
*/

#include "database.h"
#include <libpq-fe.h>
#include <string.h>

PGconn     *conn=NULL;
static void exit_nicely(PGconn *c)
{
    PQfinish(c);
}
int init_database(char* dbstr){
	conn = PQconnectdb(dbstr);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s",
                PQerrorMessage(conn));
        exit_nicely(conn);
		return -1;
    }
 	return 0;
}

int release_database(){
	exit_nicely(conn);
	return 0;
}