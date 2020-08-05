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
//这里是可以再多加一些其它能力进去，故而只是一句，也用了一个函数
static void exit_nicely(PGconn *c)
{
	PQfinish(c);
}
//初始化数据库
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
//释放数据库
int release_database(){
	exit_nicely(conn);
	return 0;
}

//验证是不是黑名单里
int check_blank_list(const char* callin_number,const char* group_number){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT id  FROM call_blacklist where call_number='" + callnumber + "' and group_number ='" +group_number+ "'");
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

//查询组的呼叫模式和超时时长
int get_group_call_mode_and_timeout(const char* group_number,int* mode,int* timeout){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT group_call_mode ,group_callout_timeout FROM ext_group where group_number  ='" +group_number+ "'"); 
	//理论上只有一条
	res = PQexec(conn,cmd);


	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		fprintf(stderr,"Exec Query Failed!\\n");
		PQclear(res);
		return 0;
	}   
	i = PQntuples(res);
	t = PQnfields(res);
	char tmp[20];
	int return_val=-1;
	for(int s=0; s<i;s++) {
		for (k = 0; k<t; k++) {

			sprintf(tmp,PQgetvalue(res,s,k));
			if (k == 0){
				*mode = atoi(tmp);
			}else{
				*timeout = atoi(tmp);
			}

		}
		return_val = 0;
		break;
	}  
	PQclear(res);
	return return_val;

}
int get_group_current_ext(const char* group_number,char* ext){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT current_ext_number  FROM ext_group where group_number  ='" +group_number+ "'"); 
	//理论上只有一条
	res = PQexec(conn,cmd);


	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		fprintf(stderr,"Exec Query Failed!\\n");
		PQclear(res);
		return 0;
	}   
	i = PQntuples(res);
	t = PQnfields(res);

	int return_val=-1;
	for(int s=0; s<i;s++) {
		for (k = 0; k<t; k++) {
			sprintf(*ext,PQgetvalue(res,s,k));
		}
		return_val = 0;
		break;
	}  
	PQclear(res);
	return return_val;
}

int get_last_answer_ext(const char* callin_number,const char* group_number,char* ext){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT extension_number  FROM call_extension where reg_state='reded' and seat_status='idle' and call_state='ready' and extension_number  = (select agent_number from cf_call_remember where call_number='" +callin_number+ "' order by insert_time desc limit 1 )"); 
	//理论上只有一条
	res = PQexec(conn,cmd);


	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		fprintf(stderr,"Exec Query Failed!\\n");
		PQclear(res);
		return 0;
	}   
	i = PQntuples(res);
	t = PQnfields(res);

	int return_val=-1;
	for(int s=0; s<i;s++) {
		for (k = 0; k<t; k++) {
			sprintf(ext,PQgetvalue(res,s,k));
		}
		return_val = 0;
		break;
	}  
	PQclear(res);
	return return_val;
}

//group_number 组名及组短号，12345热线就12345
//ext 空闲的座席
//timeout 要呼叫的超时时长
//返回值，有空闲的，那么就返回0及ext为空闲座席值
int get_group_idle_ext_first(const char* callin_number,const char* group_number,char* ext,int* timeout){
	//这里查找排队类型，如果是循环排队，以及记忆排队，那先优先检测记录排队，如果该座席忙，则再进行排队类型的排队，而循 环则是
	//要取出当前排队的分机，再按分机序号进行下一个
	int mode;
	int ret_val=get_group_call_mode_and_timeout(group_number,&mode,timeout);
	if (ret_val == 0 )
	{
		if (mode>2){
			//存在记忆呼叫
			ret_val =  get_last_answer_ext(callin_number,group_number,ext);
			if (ret_val !=0){
				//需要另外按mode进行数据库查询
				char cmd[500];
				PGresult *res;
				int i = 0,t = 0,s,k;
				if (mode ==0 || mode ==3){
					sprintf(cmd,"select c.extension_number from call_extension c,ext_group e where (c.reg_state='reged' OR c.reg_state='REGED') and 
							(c.call_state='ready' OR c.call_state='READY') and (c.seat_state='up' OR c.seat_state='UP') and (c.seat_status='ready' OR c.seat_status='idle') 
							and (e.group_number='%s') and (c.extension_number in ( select ext from ext_group_map where ext_group_number='%s'
									)) order by c.extension_number limit 1",group_number,group_number);
				}else if(mode == 1 || mode==4){
					sprintf(cmd,"select c.extension_number from call_extension c,ext_group e where (c.reg_state='reged' OR c.reg_state='REGED') and 
							(c.call_state='ready' OR c.call_state='READY') and (c.seat_state='up' OR c.seat_state='UP') and (c.seat_status='ready' OR c.seat_status='idle') 
							and (e.group_number='%s') and (c.extension_number in ( select ext from ext_group_map where ext_group_number='%s'
									)) order by random() limit 1",group_number,group_number);
				}else if(mode ==2 || mode==5){
					sprintf(cmd,"select c.extension_number from call_extension c,ext_group e where (c.reg_state='reged' OR c.reg_state='REGED') and 
							(c.call_state='ready' OR c.call_state='READY') and (c.seat_state='up' OR c.seat_state='UP') and (c.seat_status='ready' OR c.seat_status='idle') 
							and (c.extension_number >e.current_ext_number) and (e.group_number='%s') and (c.extension_number in ( select ext from ext_group_map where ext_group_number='%s'
									)) order by c.extension_number limit 1",group_number,group_number);
				}
				res = PQexec(conn,cmd);

				if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
					fprintf(stderr,"Exec Query Failed!\\n");
					PQclear(res);
					return 0;
				}   
				i = PQntuples(res);
				t = PQnfields(res);

				int return_val=-1;
				for(int s=0; s<i;s++) {
					for (k = 0; k<t; k++) {
						sprintf(ext,PQgetvalue(res,s,k));
					}
					return_val = 0;
					break;
				}  
				PQclear(res);
			}
		}
	}
	return ret_val;
}
int update_ext_busy(const char* ext){
	PGresult *res;
	char cmd[4000];
	int return_val=-1;
	sprintf(cmd,"update call_extension set call_state='talking' where extension_number ='%s';",ext);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "update extensio to busy failed: %s", PQerrorMessage(conn));
		PQclear(res);

	}
	PQclear(res);
	return return_val;

}
int update_ext_idle(const char* ext){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];

	sprintf(cmd,"update call_extension set call_state='idle' where extension_number ='%s';",ext);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "update extensio to idle failed: %s", PQerrorMessage(conn));
		PQclear(res);

	}
	PQclear(res);
	return return_val;
}
int check_vip_list(const char* callin_number,const char* group_number){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT id  FROM call_vip_number where phone_number ='" + callnumber + "' and group_number ='" +group_number+ "'");
	res = PQexec(conn,cmd);


	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		fprintf(stderr,"Exec Query Failed!\\n");
		PQclear(res);
		return PQresultStatus(res);
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
