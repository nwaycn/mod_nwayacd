/*
   --------mod_nwayacd
   --------mod_nwayacd.c
   --------座席排队数据表
   --------许可协议：Apache-2.0
   --------开发团队：上海宁卫信息技术有限公司
   --------联系Email:lihao@nway.com.cn
   --------开始开发时间：2020-8-1 中华人民共和国建军节
 */

#include <switch.h>
#include "database.h"
#define AGENT_INFO "nway::info"
#define AGENT_CALLIN "nway_callin"
#define AGENT_CALLOUT "nway_callout"

SWITCH_MODULE_LOAD_FUNCTION(mod_nwayacd_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_nwayacd_shutdown);
SWITCH_MODULE_DEFINITION(mod_nwayacd, mod_nwayacd_load, mod_nwayacd_shutdown, NULL);
#define BLACKLIST_FILE "/home/blacklist.wav"
#define AGENT_BUSY "/home/busy.wav"
static struct {

	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	unsigned int fs_ver;
	PGconn   *db_connection;
	char *db_info;
	int db_online;
	//char dbstring[255];   // read config data when load

} globals; 

//caller number 
struct acd_caller {
	char *username;
	int caller_type;
};

typedef struct acd_caller acd_caller_t;

inline bool check_pq(){
	if (!globals.db_online || PQstatus(globals.db_connection) != CONNECTION_OK) {
		globals.db_connection = PQconnectdb(globals.db_info);
	}

	if (PQstatus(globals.db_connection) == CONNECTION_OK) {
		globals.db_online = 1;
	} else {
		return false;
	}
	return true;
}

static void acd_get_caller(acd_caller_t *caller, const char *channel_name)
{
	char *p;
	if (!strncmp(channel_name, "sofia/external", 14))
	{
		p = strchr(channel_name + 14, '/');
		if (p)
		{
			caller->username = p + 1;
			caller->caller_type = 1;
		}
	}
	else if (!strncmp(channel_name, "sofia/internal/", 15))
	{
		caller->username = (char *)channel_name + 15;
		caller->caller_type = 0;
	}
	if (caller->username && (p = strchr(caller->username, '@')))
	{
		*p++ = '\0';
	}
}

static switch_status_t nway_hook_state_run(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	const char *agent_name = NULL;
	const char *bill_sec=NULL;

	agent_name = switch_channel_get_variable(channel, AGENT_CALLOUT);
	bill_sec = switch_channel_get_variable(channel, "bill_sec");
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Called hanguphook channel %s with state %s", switch_channel_get_name(channel), switch_channel_state_name(state));

	if (state == CS_HANGUP) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Tracked call for agent %s ended,bill_sec:%s ", agent_name,bill_sec);

		switch_core_event_hook_remove_state_run(session, nway_hook_state_run);
		if (check_pq())
			update_ext_idle(agent_name,globals.db_connection);
		//需要置闲，同时判断是否被agent接听了，如果是被接听了，则不做处理，如果是没接听，则转下一个座席
	}

	return SWITCH_STATUS_SUCCESS;
}

//主执行函数，用于对来电号码进行排队和处理
switch_status_t nwayacd(switch_core_session_t *session, const char* group_number,switch_stream_handle_t *stream){
	 
	char* uuid=switch_core_session_get_uuid(session); 
	const char *dest_num = NULL;
	char* cmd=NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	const char *channel_name = switch_channel_get_variable(channel, "channel_name");
	acd_caller_t caller = { 0 }; 
	int timeout=0;
	int ret_val =0;
	if (!zstr(group_number)) {
		//	group_number = switch_core_session_strdup(session, data);

	}else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No Destination number provided\n");
		goto end;
	}
	if (!zstr(channel_name))
	{
		acd_get_caller(&caller, channel_name);
		if (!zstr(caller.username))
		{
			//here to check black list
			if (check_pq()){
				if (check_blank_list(caller.username,group_number,globals.db_connection) == 0){
					//
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Sorry, this call is not permitted! [%s]\n", caller.username);
					switch_ivr_sleep(session, 500, SWITCH_TRUE, NULL);
					switch_ivr_play_file(session, NULL,BLACKLIST_FILE , NULL);
					goto end;
				}
			}

		}
	}
	//here to query idle agent in group
	char ext[20];
	if (!check_pq()) goto end;
	ret_val = get_group_idle_ext_first(caller.username,group_number,ext,&timeout,globals.db_connection);
	if (ret_val==0){
		//has an idle agent extension
		//采用呼叫后通过uuid转，先注释
		cmd = switch_mprintf("{ignore_early_media=true,originate_timeout=%d,origination_caller_id_number=%s}user/%s",
				timeout,caller.username,ext);
		//cmd = switch_mprintf("{ignore_early_media=true,originate_timeout=%d}user/%s &nway_bridge(%s)",timeout,ext,uuid);
		//switch_api_execute("originate", cmd, NULL, stream);
		switch_core_session_t *new_session = NULL;
		switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
		switch_status_t status = SWITCH_STATUS_FALSE;
		switch_event_t *nway_event = NULL;
		status = switch_ivr_originate(session, &new_session, &cause, cmd, 0, NULL, NULL, NULL, NULL, nway_event, SOF_NONE, NULL);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "call out string:%s\n",cmd);
		if (status || !new_session) {
			const char *fail_str = switch_channel_cause2str(cause);
			switch_event_t *event;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Originate Failed.  Cause: %s\n", fail_str);
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, AGENT_INFO) == SWITCH_STATUS_SUCCESS) {
				switch_time_t obc_callfrom_etime = switch_micro_time_now() / 1000000;
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, AGENT_CALLIN, caller.username);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, AGENT_CALLOUT, ext);

				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "nway_callstatus", "1");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "nway_callfailcode", "%d", cause);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "nway_callfrom_etime", "%" SWITCH_TIME_T_FMT, obc_callfrom_etime);
				switch_event_fire(&event);
			}
			if (stream)	stream->write_function(stream, "-ERR %s\n", switch_channel_cause2str(cause));
			goto end;
		}else {
			//需要设置该分机为忙
			if (!check_pq()) goto end;
			update_ext_busy(ext,globals.db_connection);
			switch_channel_t *channel = switch_core_session_get_channel(new_session);
			switch_event_t *event;
			switch_caller_extension_t *extension = NULL;
			switch_channel_set_variable_printf(channel, "nway_callfrom_stime", "%" SWITCH_TIME_T_FMT, switch_micro_time_now() / 1000000);
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, AGENT_INFO) == SWITCH_STATUS_SUCCESS) {

				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, AGENT_CALLIN, caller.username);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, AGENT_CALLOUT, ext); 
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "nway_callstatus", "0");
				switch_event_fire(&event);
			}
			//switch_channel_set_variable_safe(channel, SWITCH_SESSION_IN_HANGUP_HOOK_VARIABLE, "true");
			//switch_channel_set_variable_safe(channel, SWITCH_API_REPORTING_HOOK_VARIABLE, "hangup_handlers");

			if ((extension = switch_caller_extension_new(new_session, "nwaycall", caller.username)) == 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
				abort();
			}
			//添加对呼叫的回调，用来判断是否呼叫成功
			switch_core_event_hook_add_state_run(new_session, nway_hook_state_run);
			if (stream)	stream->write_function(stream, "+OK %s\n", switch_core_session_get_uuid(new_session));
			switch_core_session_rwunlock(new_session);
		}
		goto end;


	}else{
		//if agents are busy,then insert into queue
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Sorry, no idle agent! [%s]\n", caller.username);
		switch_ivr_sleep(session, 500, SWITCH_TRUE, NULL);
		switch_ivr_play_file(session, NULL,AGENT_BUSY , NULL);
		//应把未能呼转的加进排队队列中

	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "%s waiting for an idle agent\n", switch_channel_get_name(channel));

end:
	switch_safe_free(cmd);
	return SWITCH_STATUS_SUCCESS;
}

//nwayacd group_number
SWITCH_STANDARD_APP(nwayacd_function){
	char *group_number = NULL;
	if (!zstr(data)) {
		group_number = switch_core_session_strdup(session, data);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "group number:%s\n",group_number);
	}else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No Destination number provided \n");
		goto end;
	}
	nwayacd(session,group_number,NULL);
end:
	return;   

}

//nway bridge to a uuid
//&nway_bridge(uuid)
SWITCH_STANDARD_APP(nway_bridge_function){
	//switch_channel_t *channel = switch_core_session_get_channel(session);

	char* uuid=switch_core_session_get_uuid(session);
	char* myuuid = switch_core_session_strdup(session, data);
	//这里调用uuid_bridge相关函数即可
	switch_ivr_uuid_bridge(uuid,myuuid);
	return;   

}


#define UUID_NWAYACD_SYNTAX "nwayacd uuid group_number\nusage: nwayacd 1-4-nway-uuid 12345"
SWITCH_STANDARD_API(uuid_nwayacd_function)
{
	switch_core_session_t *rsession = NULL;
	char *mycmd = NULL,  *argv[3] = { 0 },  *uuid = NULL, *group_number=NULL;
	int argc = 0, type = 1;

	if (zstr(cmd)) {
		goto usage;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cmd [%s]\n", cmd);
	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		goto usage;
	}

	uuid = argv[0];
	group_number = argv[1];
	if (uuid && strlen(uuid)>1){
		if (!(rsession = switch_core_session_locate(uuid))) {
			stream->write_function(stream, "-ERR Cannot locate session!\n");
			goto done;
		}
		nwayacd(rsession,group_number,stream);

	}


done:
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
usage:
	switch_safe_free(mycmd);
	stream->write_function(stream, "-USAGE: %s\n", UUID_NWAYACD_SYNTAX);
	return SWITCH_STATUS_FALSE;

}



static switch_status_t load_config(void)
{
	char *cf = "nwayacd.conf";
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_uuid_str(uuid_str, sizeof(uuid_str));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, uuid_str);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "dbstring")) {
				if (!zstr(val)) {

					//strncpy(globals.dbstring,val,255);
					switch_strdup(globals.db_info,val);
				}
			}

		}
	}
done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}


#define NWAY_LOGIN_SYNTAX "nway_login extension group_list\nUsage: nway_login 1000 120,119,110"
SWITCH_STANDARD_API(nway_login_function)
{
	//最大10个分组	
	char *mycmd = NULL,  *argv[3] = { 0 },  *extension = NULL, *group_number[10]={0},*mygroup=NULL;
	int argc = 0, type = 1;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cmd [%s]\n", cmd);
	if (zstr(cmd)) {

		goto usage;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cmd [%s]\n", cmd);
	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "argc [%d]\n", argc);
		goto usage;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "argc [%d], 0:[%s]\n", argc,argv[0]);
	extension = argv[0];
	mygroup = strdup(argv[1]);
	if (zstr(mygroup)){
		goto usage;
	}
	if ((argc = switch_separate_string(mygroup, ',', group_number, (sizeof(group_number) / sizeof(group_number[0])))) <1) {
		goto usage;
	}
	//先要让上线
	if (!check_pq()) goto done;
	//重新注册时需要清空原来的对应关系，用于万一之前没调用登出，引起登入数据太多
	if (nway_remove_from_group(extension,globals.db_connection) == 0){

	}else switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "remove all ext :%s map info failed\n", extension);
	if (nway_agent_online(extension,globals.db_connection) == 0){
		//这里需要ｅｓｌ事件
	}else{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "login extensio and online:%s failed\n", extension);
	}
	//按group数量插入座席组与座席对应表

	for (int i=0;i<argc;i++){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "login extension:%s, group:%s\n", extension,group_number[i]);
		if (nway_add_to_group(extension,group_number[i],globals.db_connection) == 0){
			//esl
		}else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "login extension:%s, group:%s add to group failed\n", extension,group_number[i]);
		}
	}
	goto done;

done:
	switch_safe_free(mygroup);
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS; 
usage:
	stream->write_function(stream, "-USAGE: %s\n", NWAY_LOGIN_SYNTAX);
	switch_safe_free(mygroup);
	switch_safe_free(mycmd);
	return SWITCH_STATUS_FALSE; 
}
#define NWAY_LOGOUT_SYNTAX "nway_logout extension \nUsage: nway_logout 1000"
SWITCH_STANDARD_API(nway_logout_function)
{
	//最大10个分组	
	char *mycmd = NULL,  *argv[3] = { 0 },  *extension = NULL ;
	int argc = 0, type =   1;

	if (zstr(cmd)) {
		goto usage;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cmd [%s]\n", cmd);
	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
		goto usage;
	}

	extension = argv[0];
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "logout extension:%s\n", extension);
	if (!check_pq()) goto done;
	if (nway_agent_offline(extension,globals.db_connection)==0){
		//esl
	}


done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
usage:
	stream->write_function(stream, "-USAGE: %s\n", NWAY_LOGOUT_SYNTAX);
	switch_safe_free(mycmd);
	return SWITCH_STATUS_FALSE;
}

#define NWAY_BUSY_SYNTAX "nway_busy extension \nUsage:nway_busy 1000"
SWITCH_STANDARD_API(nway_busy_function)
{

	char *mycmd = NULL,  *argv[3] = { 0 },  *extension = NULL ;
	int argc = 0, type = 1;

	if (zstr(cmd)) {
		goto usage;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cmd [%s]\n", cmd);
	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
		goto usage;
	}

	extension = argv[0];
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "set extension:%s busy\n", extension);
	if (!check_pq()) goto done;
	if (nway_agent_set_busy(extension,globals.db_connection) ==0){
		//esl
	}


done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
usage:
	stream->write_function(stream, "-USAGE: %s\n", NWAY_BUSY_SYNTAX);
	switch_safe_free(mycmd);
	return SWITCH_STATUS_FALSE;
}
#define NWAY_READY_SYNTAX "nway_ready extension \nUsage:nway_ready 1000"
SWITCH_STANDARD_API(nway_ready_function)
{

	char *mycmd = NULL,  *argv[3] = { 0 },  *extension = NULL ;
	int argc = 0, type = 1;

	if (zstr(cmd)) {
		goto usage;
	}
	switch_log_printf( SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cmd [%s]\n", cmd);
	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) <1) {
		goto usage;
	}

	extension = argv[0];
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "set extension:%s ready\n", extension);
	if (!check_pq()) goto done;
	if (nway_agent_set_ready(extension,globals.db_connection) ==0){
		//esl
	}

done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
usage:
	stream->write_function(stream, "-USAGE: %s\n", NWAY_READY_SYNTAX);
	switch_safe_free(mycmd);
	return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_nwayacd_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	globals.db_online = 0;
	load_config();
	if (check_pq()){
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Connection to database failed: %s", PQerrorMessage(globals.db_connection));
		goto error;
	}
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	// unsigned int major = atoi(switch_version_major());
	// unsigned int minor = atoi(switch_version_minor());
	// unsigned int micro = atoi(switch_version_micro());

	// globals.fs_ver = major << 16;
	// globals.fs_ver |= minor << 8;
	// globals.fs_ver |= micro << 4;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "nwayacd", "nwayacd", "nwayacd", nwayacd_function,
			"nway acd ", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "nway_bridge", "nway_bridge", "nway_bridge", nway_bridge_function,
			"nway bridge to a leg uuid ", SAF_NONE);

	SWITCH_ADD_API(api_interface, "nwayacd", "nway acd", uuid_nwayacd_function, UUID_NWAYACD_SYNTAX);
	//迁入 and 绑定座席组
	SWITCH_ADD_API(api_interface, "nway_login", "nway_login agent login system and add to group", nway_login_function, NWAY_LOGIN_SYNTAX);
	//迁出
	SWITCH_ADD_API(api_interface, "nway_logout", "nway_logout an agent", nway_logout_function, NWAY_LOGOUT_SYNTAX);
	//置忙
	SWITCH_ADD_API(api_interface, "nway_busy", "nway_busy it no phone call in", nway_busy_function, NWAY_BUSY_SYNTAX);
	//置闲,状态为等待来电
	SWITCH_ADD_API(api_interface, "nway_ready", "nway_ready it wait for phone", nway_ready_function, NWAY_READY_SYNTAX);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " module nway acd loaded\n");
	return SWITCH_STATUS_SUCCESS;
error:
	PQfinish(globals.db_connection);
	globals.db_online = 0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " module nway acd failed\n");
	return SWITCH_STATUS_FALSE;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_nwayacd_shutdown)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " nwayacd_shutdown\n");
	if (PQstatus(globals.db_connection) == CONNECTION_OK) {
		PQfinish(globals.db_connection);
	}
	switch_mutex_destroy(globals.mutex);
	return SWITCH_STATUS_SUCCESS;
}

