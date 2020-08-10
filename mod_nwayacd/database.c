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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>

//这里是可以再多加一些其它能力进去，故而只是一句，也用了一个函数


//验证是不是黑名单里
int check_blank_list(const char* callin_number,const char* group_number,PGconn *conn){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT id  FROM call_blacklist where call_number='%s' and group_number ='%s' limit 1;", callin_number ,group_number );
	res = PQexec(conn,cmd);


	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "check_blank_list failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return 0;
	}

	i = PQntuples(res);

	t = PQnfields(res);
	char id[20];
	int return_val=-1;
	for(int s=0; s<i;s++) {


		sprintf(id,"%s",PQgetvalue(res,s,0));
		return_val = 0;

	}

	PQclear(res);
	return return_val;
}

//查询组的呼叫模式和超时时长
int get_group_call_mode_and_timeout(const char* group_number,int* mode,int* timeout,PGconn *conn){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT group_call_mode ,group_callout_timeout FROM ext_group where group_number  ='%s';",group_number); 
	//理论上只有一条
	res = PQexec(conn,cmd);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "get_group_call_mode_and_timeout cmd: %s\n",cmd);
	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "get_group_call_mode_and_timeout failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return -1;
	}   
	i = PQntuples(res);
	t = PQnfields(res);
	char tmp[20];
	int return_val=-1;
	for(int s=0; s<i;s++) {
		for (k = 0; k<t; k++) {

			sprintf(tmp,"%s",PQgetvalue(res,s,k));
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
int get_group_current_ext(const char* group_number,char* ext,PGconn *conn){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT current_ext_number  FROM ext_group where group_number  ='%s' limit 1;" ,group_number); 
	//理论上只有一条
	res = PQexec(conn,cmd);


	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "get_group_current_ext failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return 0;
	}   
	i = PQntuples(res);
	t = PQnfields(res);

	int return_val=-1;
	for(int s=0; s<i;s++) {

		sprintf(ext,"%s",PQgetvalue(res,s,0));

		return_val = 0;

	}  
	PQclear(res);
	return return_val;
}

int get_last_answer_ext(const char* callin_number,const char* group_number,char* ext,PGconn *conn){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT extension_number  FROM call_extension where reg_state='reded' and seat_status='idle' and call_state='ready' and extension_number  = (select agent_number from cf_call_remember where call_number='%s' order by insert_time desc limit 1 )",callin_number); 
	//理论上只有一条
	res = PQexec(conn,cmd);


	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "get_last_answer_ext failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return 0;
	}   
	i = PQntuples(res);
	t = PQnfields(res);

	int return_val=-1;
	for(int s=0; s<i;s++) {

		sprintf(ext,"%s",PQgetvalue(res,s,0));

		return_val = 0;
	}  
	PQclear(res);
	return return_val;
}

//group_number 组名及组短号，12345热线就12345
//ext 空闲的座席
//timeout 要呼叫的超时时长
//返回值，有空闲的，那么就返回0及ext为空闲座席值
int get_group_idle_ext_first(const char* callin_number,const char* group_number,char* ext,int* timeout,PGconn *conn){
	//这里查找排队类型，如果是循环排队，以及记忆排队，那先优先检测记录排队，如果该座席忙，则再进行排队类型的排队，而循 环则是
	//要取出当前排队的分机，再按分机序号进行下一个
	int mode;
	int ret_val=get_group_call_mode_and_timeout(group_number,&mode,timeout,conn);
	if (ret_val == 0 )
	{
		if (mode>2){
			//存在记忆呼叫
			ret_val =  get_last_answer_ext(callin_number,group_number,ext,conn);
			if (ret_val !=0){
				goto acd;
			}
			return 0;
		}
acd:
	//需要另外按mode进行数据库查询
		char cmd[2000];
		PGresult *res;
		int i = 0,t = 0,s,k;
		if (mode ==0 || mode ==3){
			sprintf(cmd,"select c.extension_number from call_extension c,ext_group e where (c.reg_state='reged' OR c.reg_state='REGED') and "
					"(c.call_state='ready' OR c.call_state='READY') and (c.seat_state='up' OR c.seat_state='UP') and (c.seat_status='ready' OR c.seat_status='idle') "
					" and (e.group_number='%s') and (c.extension_number in ( select ext from ext_group_map where ext_group_number='%s' "
					")) order by c.extension_number limit 1",group_number,group_number);
		}else if(mode == 1 || mode==4){
			sprintf(cmd,"select c.extension_number from call_extension c,ext_group e where (c.reg_state='reged' OR c.reg_state='REGED') and "
					"(c.call_state='ready' OR c.call_state='READY') and (c.seat_state='up' OR c.seat_state='UP') and (c.seat_status='ready' OR c.seat_status='idle') "
					"and (e.group_number='%s') and (c.extension_number in ( select ext from ext_group_map where ext_group_number='%s'"
					")) order by random() limit 1",group_number,group_number);
		}else if(mode ==2 || mode==5){
			sprintf(cmd,"select c.extension_number from call_extension c,ext_group e where (c.reg_state='reged' OR c.reg_state='REGED') and "
					"(c.call_state='ready' OR c.call_state='READY') and (c.seat_state='up' OR c.seat_state='UP') and (c.seat_status='ready' OR c.seat_status='idle') "
					"and (c.extension_number >e.current_ext_number) and (e.group_number='%s') and (c.extension_number in ( select ext from ext_group_map where ext_group_number='%s'"
					")) order by c.extension_number limit 1",group_number,group_number);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, " get_group_idle_ext_first cmd: %s\n",cmd);
		res = PQexec(conn,cmd);

		if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, " get_group_idle_ext_first failed: %s\n",PQerrorMessage(conn));
			PQclear(res);
			return -2;
		}   
		i = PQntuples(res);
		t = PQnfields(res);

		 
		for(int s=0; s<i;s++) {

			sprintf(ext,"%s",PQgetvalue(res,s,0));

			ret_val = 0;
			break;
		}  
		PQclear(res);
	}
	return ret_val;
}
int update_ext_busy(const char* ext,PGconn *conn){
	PGresult *res;
	char cmd[4000];
	int return_val=-1;
	sprintf(cmd,"update call_extension set call_state='ring' where extension_number ='%s';",ext);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, " update_ext_busy failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;

}
int update_ext_talking(const char* ext,PGconn *conn){
	PGresult *res;
	char cmd[4000];
	int return_val=-1;
	sprintf(cmd,"update call_extension set call_state='talking' where extension_number ='%s';",ext);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, " update_ext_talking failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;

}
int update_ext_idle(const char* ext,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];

	sprintf(cmd,"update call_extension set call_state='ready' where extension_number ='%s';",ext);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, " update extensio to idle failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0; 
	PQclear(res);
	return return_val;
}
//查找是否vip客户，只在排队时有用
int check_vip_list(const char* callin_number,const char* group_number,PGconn *conn){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT id  FROM call_vip_number where phone_number ='%s' and group_number ='%s' limit 1;" , callin_number ,group_number);
	res = PQexec(conn,cmd);

	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, " check_vip_list failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return PQresultStatus(res);
	}

	i = PQntuples(res);
	t = PQnfields(res);
	char id[20];
	int return_val=-1;
	for(int s=0; s<i;s++) {
		sprintf(id,"%s",PQgetvalue(res,s,0));
		return_val = 0;		 
	}

	PQclear(res);
	return return_val;
}

int query_uuid_from_queue(const char* call_uuid,PGconn * conn){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT id  FROM callin_queue where call_uuid ='%s';" , call_uuid);
	res = PQexec(conn,cmd);

	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, " check_vip_list failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return PQresultStatus(res);
	}

	i = PQntuples(res);
	t = PQnfields(res);
	char id[20];
	int return_val=-1;
	for(int s=0; s<i;s++) {
		sprintf(id,"%s",PQgetvalue(res,s,0));
		return_val = 0;		 
	}

	PQclear(res);
	return return_val;
}
int insert_into_queue(const char* callin_number,const char* group_number,const char* call_uuid,PGconn *conn){
	int is_vip = 1;
	PGresult *res;
	int return_val=-1;
	char cmd[4000];
	if (query_uuid_from_queue(call_uuid,conn) == 0) return 0;
	if (check_vip_list(callin_number,group_number,conn) != 0){
		is_vip = 0;
	}
	sprintf(cmd,"INSERT INTO public.callin_queue(callin_number, callin_group, callin_type, call_time,call_uuid)VALUES ('%s','%s',%d,now(),'%s');",callin_number,group_number,is_vip,call_uuid);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "insert record into callin queue failed: %s\n",PQerrorMessage(conn));

	}else return_val = 0;
	PQclear(res);
	return return_val;
}
int delete_from_queue(const char* callin_number,const char* group_number,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];
	sprintf(cmd,"delete from callin_queue where callin_number='%s' and callin_group='%s';",callin_number,group_number);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "delete record from callin queue failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;
}

int delete_from_queue_with_uuid(const char* uuid,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];
	sprintf(cmd,"delete from callin_queue where call_uuid='%s' ;",uuid);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "delete record from callin queue failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;
}

int query_vip_callin(char* callin_number,char* group_number,char* call_uuid,PGconn *conn){

	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT callin_number,callin_group,call_uuid from callin_queue where callin_type=1 order by call_time limit 1;");
	res = PQexec(conn,cmd);

	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "query_vip_callin failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return PQresultStatus(res);
	}

	i = PQntuples(res);
	t = PQnfields(res);

	int return_val=-1;
	for(int s=0; s<i;s++) {
		sprintf(callin_number,"%s",PQgetvalue(res,s,0));
		sprintf(group_number,"%s",PQgetvalue(res,s,1));	
		sprintf(call_uuid,"%s",PQgetvalue(res,s,2));	
		return_val = 0;
	} 

	PQclear(res);
	return return_val;

}
int query_a_data_from_queue(char* callin_number,char* group_number,char* call_uuid,PGconn *conn){
	if (query_vip_callin(callin_number,group_number,call_uuid,conn) ==0){
		return 0;
	}
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT callin_number,callin_group,call_uuid from callin_queue where callin_type=0 order by call_time limit 1;");
	res = PQexec(conn,cmd);

	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "query_a_data_from_queue failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return PQresultStatus(res);
	}

	i = PQntuples(res);
	t = PQnfields(res);

	int return_val=-1;
	for(int s=0; s<i;s++) {
		sprintf(callin_number,"%s",PQgetvalue(res,s,0));
		sprintf(group_number,"%s",PQgetvalue(res,s,1));	
		sprintf(call_uuid,"%s",PQgetvalue(res,s,2));	
		return_val = 0;
	} 

	PQclear(res);
	return return_val;
}
int nway_agent_online(const char* extension,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];

	sprintf(cmd,"update call_extension set seat_state='up',reg_state='reged',call_state='ready',seat_status='idle' where extension_number='%s';",extension);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "extension up failed: %s\n",PQerrorMessage(conn));
	}else{
		return_val = 0;
	}
	PQclear(res);
	return return_val;
}
int nway_agent_offline(const char* extension,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];

	sprintf(cmd,"update call_extension set seat_state='down' where extension_number='%s';",extension);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "nway_agent_offline up failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	nway_remove_from_group(extension,conn);
	return return_val;
}
int nway_add_to_group(const char* extension,const char*  group_number,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];

	sprintf(cmd,"INSERT INTO public.ext_group_map(id, ext_group_id, ext_group_number, ext)VALUES (0,0,'%s','%s');",group_number,extension);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "nway_add_to_group cmd: %s\n",cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "nway_add_to_group  failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;
}
int nway_remove_from_group(const char* extension,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];

	sprintf(cmd,"delete from ext_group_map where ext='%s';",extension);

	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "nway_remove_from_group up failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;
}
int nway_agent_set_busy(const char* extension,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];

	sprintf(cmd,"update call_extension set seat_status='busy' where extension_number='%s';",extension);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "nway_agent_set_busy failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;
}

int nway_agent_set_ready(const char* extension,PGconn *conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];
	sprintf(cmd,"update call_extension set seat_status='idle',call_state='ready' where extension_number='%s';",extension);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "nway_agent_set_ready failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;
}
int insert_remember_cdr(const char* group_name,const char* caller,const ext,PGconn* conn){
	PGresult *res;
	int return_val=-1;
	char cmd[4000];
	sprintf(cmd,"insert into cf_call_remember(id ,call_number ,agent_number ,insert_time ,group_number )VALUES(0,'%s','%s',now(),'%s');",caller,ext,group_name);
	//fprintf(stderr,cmd);
	res = PQexec(conn, cmd);
	return_val = PQresultStatus(res) ;
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "insert_remember_cdr failed: %s\n",PQerrorMessage(conn));
	}else return_val = 0;
	PQclear(res);
	return return_val;
}
int query_group_params(const char* group_number,char* black_ring,char* transfer_ring,char* busy_ring,PGconn * conn){
	PGresult *res;
	char cmd[400];
	int i = 0,t = 0,s,k;
	sprintf(cmd,"SELECT transfer_ring,black_list_ring,busy_ring from ext_group where group_number=‘%s';",group_number);
	res = PQexec(conn,cmd);

	if(  PQresultStatus(res)  !=  PGRES_TUPLES_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "query_group_params failed: %s\n",PQerrorMessage(conn));
		PQclear(res);
		return PQresultStatus(res);
	}

	i = PQntuples(res);
	t = PQnfields(res);

	int return_val=-1;
	for(int s=0; s<i;s++) {
		sprintf(transfer_ring,"%s",PQgetvalue(res,s,0));
		sprintf(black_ring,"%s",PQgetvalue(res,s,1));	
		sprintf(busy_ring,"%s",PQgetvalue(res,s,2));	
		return_val = 0;
	} 

	PQclear(res);
	return return_val;
}