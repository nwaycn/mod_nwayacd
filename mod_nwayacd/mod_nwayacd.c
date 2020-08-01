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

static struct {

	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	unsigned int fs_ver;
	char dbstring[255];   // read config data when load
	 
} globals; 
 
//nwayacd group_number
SWITCH_STANDARD_APP(nwayacd_function){
     
	char *group_number = NULL;
	//cr_route_t *route = NULL;
	const char *dest_num = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *sql = NULL;
	const char *channel_name = switch_channel_get_variable(channel, "channel_name");
	 

	if (!zstr(data)) {
		group_number = switch_core_session_strdup(session, data);
		 
	}else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No Destination number provided\n");
		goto end;
	}
    //here to check black list

    //here to query idle agent in group

    //if agents are busy,then insert into queue
	

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "%s has waiting for an idle agent\n", switch_channel_get_name(channel));

end:

	return;
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
	 

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " module nway acd loaded\n");
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_nwayacd_shutdown)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " nwayacd_shutdown\n");
	 
	switch_mutex_destroy(globals.mutex);
	return SWITCH_STATUS_SUCCESS;
}

