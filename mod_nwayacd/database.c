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
int check_blank_list(const char* callin_number,const char* group_number){
    PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT id  FROM call_blacklist where call_number='" + callnumber + "' and group_number ='" + "'");
	res = PQexec(conn,cmd);
	
	 
   if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
       fprintf(stderr,"Exec Query Failed!\\n");
       PQclear(res);
	   return 0;
   }
     
   i = PQntuples(res);
    
   t = PQnfields(res);
   char id[20];
   int return_val=-1;
   for(int s=0; s<i;s++) {
       for (k = 0; k<t; k++) {
            
		   sprintf(id,PQgetvalue(res,s,k));
           return_val = 0;
           break;
          
       }
       break;
   }
    
   PQclear(res);
   return return_val;
}