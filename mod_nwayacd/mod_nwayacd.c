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
//#include <libpq-fe.h>
#include "database.h"
SWITCH_MODULE_LOAD_FUNCTION(mod_nwayacd_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_nwayacd_shutdown);
SWITCH_MODULE_DEFINITION(mod_nwayacd, mod_nwayacd_load, mod_nwayacd_shutdown, NULL);
#define BLACKLIST_FILE "/home/blacklist.wav"
static struct {

	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	unsigned int fs_ver;
	char dbstring[255];   // read config data when load
	 
} globals; 

//caller number 
struct acd_caller {
	char *username;
	int caller_type;
};

typedef struct acd_caller acd_caller_t;

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

switch_status_t nwayacd(switch_core_session_t *session, const char* group_name){
    char *group_number = NULL;
	 
	const char *dest_num = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	 
	const char *channel_name = switch_channel_get_variable(channel, "channel_name");
	acd_caller_t caller = { 0 }; 

	if (!zstr(data)) {
		group_number = switch_core_session_strdup(session, data);
		 
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
             if (check_blank_list(caller.username,group_number) == 0){
                 //
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Sorry, this call is not permitted! [%s]\n", caller.username);
				switch_ivr_sleep(session, 500, SWITCH_TRUE, NULL);
				switch_ivr_play_file(session, NULL,BLACKLIST_FILE , NULL);
				goto end;
             }
        }
    }
    //here to query idle agent in group
	char ext[20];
	int timeout=0;
	int ret_val = get_group_idle_ext_first(caller.username,group_number,ext,&timeout);
	if (ret_val==0){
		//has an idle agent extension
	}else{
		//if agents are busy,then insert into queue
	}
    
	

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "%s waiting for an idle agent\n", switch_channel_get_name(channel));

end:

	 
    return SWITCH_STATUS_SUCCESS;
}

//nwayacd group_number
SWITCH_STANDARD_APP(nwayacd_function){
     char *group_number = NULL;
	 
	const char *dest_num = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	 
	const char *channel_name = switch_channel_get_variable(channel, "channel_name");
	acd_caller_t caller = { 0 }; 

	if (!zstr(data)) {
		group_number = switch_core_session_strdup(session, data);
		 
	}else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No Destination number provided\n");
		goto end;
	}
    nwayacd(session,group_name);
 end:
    return;   
	
}


#define UUID_NWAYACD_SYNTAX "nwayacd uuid group_number"
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

	uuid = argv[1];
    group_number = argv[2];
    if (uuid && strlen(uuid)>1){
         if (!(rsession = switch_core_session_locate(uuid))) {
            stream->write_function(stream, "-ERR Cannot locate session!\n");
            goto done;
        }
       return nwayacd(rsession,group_number);

    }
   
usage:
	stream->write_function(stream, "-USAGE: %s\n", MAKECALL_SYNTAX);

done:
	switch_safe_free(mycmd);
	 
	return SWITCH_STATUS_SUCCESS;
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
					 
					strncpy(globals.dbstring,val,255);

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

SWITCH_MODULE_LOAD_FUNCTION(mod_nwayacd_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;
 
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	load_config();
    if (init_database(globals.dbstring) != 0)
    {        
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, " Connection to database failed: %s\n");
         
		return SWITCH_STATUS_FALSE;
    }
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	unsigned int major = atoi(switch_version_major());
	unsigned int minor = atoi(switch_version_minor());
	unsigned int micro = atoi(switch_version_micro());

	globals.fs_ver = major << 16;
	globals.fs_ver |= minor << 8;
	globals.fs_ver |= micro << 4;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "nwayacd", "nwayacd", "nwayacd", nwayacd_function,
		"nway acd ", SAF_NONE);
	 
    SWITCH_ADD_API(api_interface, "nwayacd", "nwayacd", uuid_nwayacd_function, UUID_NWAYACD_SYNTAX);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " module nway acd loaded\n");
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_nwayacd_shutdown)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " nwayacd_shutdown\n");
	 
	switch_mutex_destroy(globals.mutex);
	return SWITCH_STATUS_SUCCESS;
}

