/*
   --------mod_nwayacd
   --------database.h
   --------座席排队数据表
   --------许可协议：Apache-2.0
   --------开发团队：上海宁卫信息技术有限公司
   --------联系Email:lihao@nway.com.cn
   --------开始开发时间：2020-8-1 中华人民共和国建军节
 */


#ifndef _NWAY_DATABASE__H
#define _NWAY_DATABASE__H

#ifdef __cplusplus  
extern "C" {  
#endif  

	int init_database(char* dbstr);
	int release_database();
	int check_blank_list(const char* callin_number,const char* group_number);
	int get_group_call_mode_and_timeout(const char* group_number,int* mode,int* timeout);
	int get_group_current_ext(const char* group_number,char* ext);
	int get_group_idle_ext_first(const char* callin_number,const char* group_number,char* ext,int* timeout);
	int update_ext_busy(const char* ext);
	int update_ext_idle(const char* ext);
	int check_vip_list(const char* callin_number,const char* group_number);
	//插入一个队列排队，由插入的同时判断是不是vip
	int insert_into_queue(const char* callin_number,const char* group_number);
	int delete_from_queue(const char* callin_number,const char* group_number);
	//从队列中取出一个呼入号码和组，内部进行判断是vip优先，先进先出
	int query_a_data_from_queue(char* callin_number,char* group_number);
	//int 
#ifdef __cplusplus  
}  
#endif  

#endif
