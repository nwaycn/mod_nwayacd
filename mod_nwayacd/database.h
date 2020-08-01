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

#ifdef __cplusplus  
}  
#endif  

#endif