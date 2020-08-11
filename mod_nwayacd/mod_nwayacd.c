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
//event field name
#define AGENT_INFO "nway::info"
#define AGENT_CALLIN "nway_callin"
#define AGENT_CALLOUT "nway_callout"
#define AGENT_GROUP "nway_group"
#define AGENT_STATUS "nway_callstatus"
#define NWAY_TIME "nway_status_time"

//event status 
#define CALLOUT "callout"
#define ANSWERED "answered"
#define NOANSWER "noanswer"
#define HANGUP "hangup"

//about queue
#define QUEUE_INFO "nway::queue"
#define QUEUE_ADD "nway_queue_add"
#define QUEUE_RM "nway_queue_remove"
#define QUEUE_OPERATE "nway_queue_operate"
#define QUEUE_CALLER "nway_queue_caller"
#define QUEUE_UUID "nway_queue_uuid"
#define QUEUE_EXTENSION "nway_queue_extension"
#define QUEUE_TIME "nway_queue_time"
#define QUEUE_GROUP "nway_queue_group"

SWITCH_MODULE_LOAD_FUNCTION(mod_nwayacd_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_nwayacd_shutdown);
SWITCH_MODULE_DEFINITION(mod_nwayacd, mod_nwayacd_load, mod_nwayacd_shutdown, NULL);
#define BLACKLIST_FILE "/home/blacklist.wav"
#define AGENT_BUSY "/home/busy.wav"
#define AGENT_TRANSFER "/home/transfer.wav"
switch_status_t nwayacd(switch_core_session_t *session, const char* group_number,switch_stream_handle_t *stream);
static struct {

	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	unsigned int fs_ver;
	PGconn   *db_connection;
	char *db_info;
	int db_online;
	int running;
	//char dbstring[255];   // read config data when load

} globals; 

//caller number 
struct acd_caller {
	char *username;
	int caller_type;

	//座席组呼叫模式

};
struct acd
{
	/* data */
	char* group_name;
	int group_mode;
	char *black_ring;
	char *transfer_ring;
	char *busy_ring;
	int play_state;  //0未播放，1正在放音中
};

typedef struct session_play
{
	/* data */
	switch_core_session_t *session;
	char* uuid;
	int playing;   //是否要播放

	char* file;
}session_play_t;


typedef struct acd_caller acd_caller_t;
typedef struct acd acd_t;


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
//处理排队信息，每秒处理一次
void *SWITCH_THREAD_FUNC queue_process_thread_run(switch_thread_t *thread, void *obj){
	//
	while (globals.running == 1) {
		char callin_number[50]={0};
		char call_group[50]={0};
		char uuid[80]={0};
		//取一个正在排队的信息，然后进行呼叫，当这个呼叫如果挂机需要从排队队列中删除，如果呼叫了另一个分机，也需要从排队中删除
		if (!check_pq())
		{
			switch_yield(1000000);
			continue;

		} 
		if (query_a_data_from_queue(callin_number,call_group,uuid,globals.db_connection) == 0){
			//认为是有排队的
			switch_core_session_t *session = NULL;
			if (!(session = switch_core_session_locate(uuid))) {

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "caller session can not find\n ");
				//这里需要清理一下了
				delete_from_queue_with_uuid(uuid,globals.db_connection);
			}else	{
				switch_core_session_rwunlock(session);
				nwayacd(session,call_group,NULL);
			}
		}

		switch_yield(1000000);

	}
}
void *SWITCH_THREAD_FUNC session_play_thread_run(switch_thread_t *thread, void *obj){
	session_play_t *sp = (session_play_t*)obj;
	if (sp){
		if(sp->session && sp->playing == 1){
			switch_core_session_t *session=sp->session;
			//switch_ivr_play_file(sp->session, NULL,sp->file , NULL);
			switch_channel_t *channel = switch_core_session_get_channel(session);
			switch_frame_t write_frame = { 0 };
			switch_file_handle_t play_fh = { 0 };
			int16_t *play_buf = NULL;
			uint32_t play_end = -1;
			int sample_count =0;
			switch_status_t status;
			switch_codec_t raw_codec = { 0 };
			switch_audio_resampler_t *resampler = NULL;
			switch_codec_implementation_t read_impl = { 0 };
			if (!zstr(sp->file)) {
				switch_core_session_get_read_impl(session, &read_impl);
				if (read_impl.actual_samples_per_second != 8000) {

					if (switch_resample_create(&resampler,
								read_impl.actual_samples_per_second,
								8000,
								read_impl.samples_per_packet, SWITCH_RESAMPLE_QUALITY, 1) != SWITCH_STATUS_SUCCESS) {

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to create resampler!\n");

						goto end;

					}
				}


				if (switch_core_file_open(&play_fh,
							sp->file,
							read_impl.number_of_channels,
							read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "open play file(%s) failed !\n", sp->file);
					status = SWITCH_STATUS_NOTFOUND;
					goto end;
				}
				if (globals.fs_ver >= ((1 << 16) | (6 << 8))) {

					switch_status_t(*codec_init_fun)(switch_codec_t *codec,
							const char *codec_name,
							const char *fmtp,
							const char *modname,
							uint32_t rate,
							int ms,
							int channels,
							uint32_t bitrate,
							uint32_t flags,
							const switch_codec_settings_t *codec_settings,
							switch_memory_pool_t *pool) = switch_core_codec_init_with_bitrate;


					if (codec_init_fun(&raw_codec,
								"L16",
								NULL,
								NULL,
								read_impl.actual_samples_per_second,
								read_impl.microseconds_per_packet / 1000,
								1,0, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s Audio Codec Activation Fail\n", switch_channel_get_name(channel));
						switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Audio codec activation failed");
						goto end;
					}

				}
				else {

					switch_status_t(*codec_init_fun)(switch_codec_t *codec,
							const char *codec_name,
							const char *fmtp,
							uint32_t rate,
							int ms,
							int channels,
							uint32_t bitrate,
							uint32_t flags,
							const switch_codec_settings_t *codec_settings,
							switch_memory_pool_t *pool) = switch_core_codec_init_with_bitrate;

					if (codec_init_fun(&raw_codec,
								"L16",
								NULL,
								read_impl.actual_samples_per_second,
								read_impl.microseconds_per_packet / 1000,
								1,0, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s Audio Codec Activation Fail\n", switch_channel_get_name(channel));
						switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Audio codec activation failed");
						goto end;
					}
				}
				switch_zmalloc(play_buf, SWITCH_RECOMMENDED_BUFFER_SIZE);


				write_frame.data = play_buf;
				write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
				write_frame.codec = &raw_codec;

				switch_core_session_set_read_codec(session, &raw_codec);
				switch_channel_audio_sync(channel);
				while (switch_channel_ready(channel) && sp->playing && play_end == (uint32_t)-1){
					//放音
					if ((switch_channel_test_flag(channel, CF_BREAK))) {
						switch_channel_clear_flag(channel, CF_BREAK);
						status = SWITCH_STATUS_BREAK;
						break;
					}
					switch_size_t olen = raw_codec.implementation->samples_per_packet;

					if (switch_core_file_read(&play_fh, play_buf, &olen) != SWITCH_STATUS_SUCCESS) {   
						play_end = sample_count;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Play end %s\n",sp->file);
					}
					else {

						write_frame.samples = (uint32_t)olen;
						write_frame.datalen = (uint32_t)(olen * sizeof(int16_t) * play_fh.channels);
						if ((status = switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0)) != SWITCH_STATUS_SUCCESS) {
							play_end = sample_count;

							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Play failed %s \n",sp->file);

						}
					}
					sample_count += raw_codec.implementation->samples_per_packet;
					switch_yield(read_impl.actual_samples_per_second*2 );

				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "sample count %d,%d\n",sample_count,read_impl.actual_samples_per_second);
				switch_core_session_reset(session, SWITCH_FALSE, SWITCH_TRUE);
				

				switch_core_codec_destroy(&raw_codec);


			}
end:

			if (resampler) {
				switch_resample_destroy(&resampler);
			}

			if (play_buf) {
				switch_core_file_close(&play_fh);
				free(play_buf);
			}
			//switch_core_session_rwunlock(session);
		}

	}
}
switch_thread_t* session_play_thread_start(session_play_t* sess)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
	switch_thread_create(&thread, thd_attr, session_play_thread_run, sess, globals.pool);
	return thread;
}
void queue_process_thread_start(void)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
	switch_thread_create(&thread, thd_attr, queue_process_thread_run, NULL, globals.pool);
}
static void push_event(const char* caller,const char* group_number,const char* extension,const char* status){
	switch_event_t *event;
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, QUEUE_INFO) == SWITCH_STATUS_SUCCESS) {
		switch_time_t nway_time = switch_micro_time_now() / 1000000;
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, AGENT_CALLIN, caller);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, AGENT_CALLOUT, extension); 
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, AGENT_GROUP, group_number); 
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, AGENT_STATUS,status);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, QUEUE_TIME, "%" SWITCH_TIME_T_FMT, nway_time);
		switch_event_fire(&event);
	}
}
static void push_queue_event(const char* caller,const char* group_number,const char* extension,const char* uuid,const char* status){


	switch_event_t *event;
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, AGENT_INFO) == SWITCH_STATUS_SUCCESS) {
		switch_time_t nway_time = switch_micro_time_now() / 1000000;
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, QUEUE_CALLER, caller);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, QUEUE_EXTENSION, extension); 
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, QUEUE_UUID, uuid); 
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, QUEUE_GROUP, group_number); 

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, QUEUE_OPERATE,status);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, NWAY_TIME, "%" SWITCH_TIME_T_FMT, nway_time);
		switch_event_fire(&event);
	}
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

static switch_status_t nway_hook_state_run_b(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	const char *agent_name = NULL;
	const char *bill_sec=NULL;
	const char *a_uuid=NULL;
	const char* group_number=NULL;
	const char* caller_number = NULL;
	if (switch_channel_test_flag(channel, CF_ANSWERED)){
		//有应答，那么就做个标记
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Called answered channel %s with state %s\n", switch_channel_get_name(channel), switch_channel_state_name(state));
		switch_channel_set_variable_printf(channel, "nway_answer_time", "%" SWITCH_TIME_T_FMT, switch_micro_time_now() / 1000000);
	}

	if (state == CS_HANGUP) {
		agent_name = switch_channel_get_variable(channel, AGENT_CALLOUT);
		bill_sec = switch_channel_get_variable(channel, "nway_answer_time");
		a_uuid = switch_channel_get_variable(channel,"nway_caller_uuid");
		group_number = switch_channel_get_variable(channel,"nway_group_number");
		caller_number = switch_channel_get_variable(channel,"nway_caller");
		push_event( caller_number,group_number,agent_name,HANGUP);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "hangup call for agent %s ended,bill_sec:%s\n ", agent_name,bill_sec);
		if (check_pq())
			update_ext_idle(agent_name,globals.db_connection);
		if (a_uuid && (!bill_sec || strlen(bill_sec)< 2)){
			//需要再一次去进行呼叫
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "agent not answer:%s, then call next\n ", agent_name);
			switch_core_session_t *rsession = NULL;
			if (!(rsession = switch_core_session_locate(a_uuid))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "caller session can not find\n ");

			}else	nwayacd(rsession,group_number,NULL);
		}
		switch_core_event_hook_remove_state_run(session, nway_hook_state_run_b);
		switch_core_session_rwunlock(session);
		//需要置闲，同时判断是否被agent接听了，如果是被接听了，则不做处理，如果是没接听，则转下一个座席
	}

	return SWITCH_STATUS_SUCCESS;
}
//用于放音等结束时再放音
static switch_status_t nway_hook_state_run_a(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	char* uuid=switch_core_session_get_uuid(session);  
	acd_t *nway_acd=NULL;
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Called hanguphook channel %s with state %s\n", switch_channel_get_name(channel), switch_channel_state_name(state));

	if (state == CS_HANGUP ) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "removed hook from channel %s with state %s\n ",switch_channel_get_name(channel), switch_channel_state_name(state));
		//清理原来的排队
		if (check_pq())
			delete_from_queue_with_uuid(uuid,globals.db_connection); 
		switch_core_event_hook_remove_state_run(session, nway_hook_state_run_a);
		//switch_core_session_rwunlock(session);
		//需要置闲，同时判断是否被agent接听了，如果是被接听了，则不做处理，如果是没接听，则转下一个座席
	}else{
		if (nway_acd = switch_channel_get_private(channel,"nway_acd")){

		}else{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Get channel private info failed\n");
			return SWITCH_STATUS_FALSE;
		}

		if (nway_acd->play_state == 0){
			if (switch_channel_test_flag(channel, CF_BROADCAST)) {
				switch_channel_stop_broadcast(channel);
			} else {
				switch_channel_set_flag_value(channel, CF_BREAK, 1);
			}
			switch_ivr_sleep(session, 500, SWITCH_TRUE, NULL);
			if (strlen(nway_acd->busy_ring)>1)
				switch_ivr_play_file(session, NULL,nway_acd->busy_ring , NULL);
			else
				switch_ivr_play_file(session, NULL,AGENT_BUSY , NULL);
		}
		//switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}

//主执行函数，用于对来电号码进行排队和处理
switch_status_t nwayacd(switch_core_session_t *session, const char* group_number,switch_stream_handle_t *stream){

	char* uuid=switch_core_session_get_uuid(session); 
	const char *dest_num = NULL;
	char* cmd=NULL;
	const char* nway_called_extension=NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	acd_t* nway_acd = NULL;
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
	if (nway_acd = switch_channel_get_private(channel,"nway_acd")){

	}else{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Get channel private info failed\n");
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
					if (strlen(nway_acd->black_ring)>1)
						switch_ivr_play_file(session, NULL,nway_acd->black_ring , NULL);
					else
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
	nway_called_extension = switch_channel_get_variable(channel,"nway_called_extension");
	//前后两个分机不同时，不然成为总是一直呼叫那一个座席了
	if (ret_val==0 && (!nway_called_extension ||strncmp(nway_called_extension,ext,20) != 0)){
		//has an idle agent extension
		//采用呼叫后通过uuid转，先注释
		cmd = switch_mprintf("{ignore_early_media=true,originate_timeout=%d,origination_caller_id_number=%s,%s=%s}user/%s",
				timeout,caller.username,AGENT_CALLOUT,ext,ext);
		//cmd = switch_mprintf("{ignore_early_media=true,originate_timeout=%d}user/%s &nway_bridge(%s)",timeout,ext,uuid);
		//switch_api_execute("originate", cmd, NULL, stream);
		switch_core_session_t *new_session = NULL;
		switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
		switch_status_t status = SWITCH_STATUS_FALSE;
		switch_event_t *nway_event = NULL;
		switch_channel_set_variable_printf(channel, "nway_called_extension", "%s", ext);
		push_event(caller.username,group_number,ext,CALLOUT);
		if (!check_pq()) goto end;
		update_ext_busy(ext,globals.db_connection);
		//从排队队列中清除针对这个uuid的排队信息，不一定有，但是执行总无错
		if (check_pq())
			delete_from_queue_with_uuid(uuid,globals.db_connection); 

		
		switch_yield(500); 	 
		status = switch_ivr_originate(NULL, &new_session, &cause, cmd, 0, NULL, NULL, NULL, NULL, nway_event, SOF_NONE, NULL);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "call out string:%s\n",cmd);

		if (status || !new_session) {
			const char *fail_str = switch_channel_cause2str(cause);
			//switch_event_t *event;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Originate Failed.  Cause: %s\n", fail_str);
			push_event(caller.username,group_number,ext,NOANSWER);
			if (!check_pq()) goto end;
			update_ext_idle(ext,globals.db_connection);
			if (switch_channel_test_flag(channel, CF_BROADCAST)) {
				switch_channel_stop_broadcast(channel);
			} else {
				switch_channel_set_flag_value(channel, CF_BREAK, 1);
			}
			switch_core_session_reset(session, SWITCH_FALSE, SWITCH_TRUE);
			nwayacd(session,group_number,stream);
			//switch_core_session_rwunlock(new_session); 
			if (stream)	stream->write_function(stream, "-ERR %s\n", switch_channel_cause2str(cause));
			goto end;
		}else {
			//需要设置该分机为忙
			 
			switch_yield(100); 
			if (!check_pq()) goto end;
			update_ext_talking(ext,globals.db_connection);
			switch_core_event_hook_add_state_run(new_session, nway_hook_state_run_b);

			///switch_channel_t *caller_channel = switch_core_session_get_channel(session);

			if (switch_channel_test_flag(channel, CF_BROADCAST)) {
				switch_channel_stop_broadcast(channel);
			} else {
				switch_channel_set_flag_value(channel, CF_BREAK, 1);
			}
			 
			switch_channel_t *peer_channel = switch_core_session_get_channel(new_session);
			switch_event_t *event;
			switch_caller_extension_t *extension = NULL;
			switch_channel_set_variable_printf(peer_channel, "nway_callfrom_stime", "%" SWITCH_TIME_T_FMT, switch_micro_time_now() / 1000000);
			switch_channel_set_variable_printf(peer_channel, "nway_caller_uuid", "%s", uuid);
			switch_channel_set_variable_printf(peer_channel, "nway_group_number", "%s", group_number);
			switch_channel_set_variable_printf(peer_channel, "nway_caller", "%s", caller.username);
			push_event(caller.username,group_number,ext,ANSWERED);
			 

			if ((extension = switch_caller_extension_new(new_session, "nwaycall", caller.username)) == 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
				abort();
			}
			//添加对呼叫的回调，用来判断是否呼叫成功

			switch_ivr_multi_threaded_bridge(session, new_session, NULL, NULL, NULL);
			if (stream)	stream->write_function(stream, "+OK %s\n", switch_core_session_get_uuid(new_session));
			//switch_core_session_rwunlock(new_session);
		}
		goto end;


	}else{
		 
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Sorry, no idle agent! [%s]\n", caller.username);
		//增加对a路状态回调检测
		switch_core_event_hook_add_state_run(session, nway_hook_state_run_a);

		int play_state = nway_acd->play_state;
		 
		
		nway_acd->play_state=1;
		switch_channel_set_private(channel, "nway_acd", nway_acd);
		insert_into_queue(caller.username,group_number,uuid,globals.db_connection);
		if (play_state == 0){
			if (switch_channel_test_flag(channel, CF_BROADCAST)) {
				switch_channel_stop_broadcast(channel);
			} else {
				switch_channel_set_flag_value(channel, CF_BREAK, 2);
			}
			switch_ivr_sleep(session, 500, SWITCH_TRUE, NULL);
			switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");
			if (strlen(nway_acd->busy_ring)>1)
				switch_ivr_play_file(session, NULL,nway_acd->busy_ring , NULL);
			else
				switch_ivr_play_file(session, NULL,AGENT_BUSY , NULL);
		}

		//应把未能呼转的加进排队队列中


	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "%s waiting for an idle agent\n", switch_channel_get_name(channel));

end:
	switch_safe_free(cmd);
	switch_core_session_reset(session, SWITCH_FALSE, SWITCH_TRUE);
	//switch_core_session_rwunlock(session);
	return SWITCH_STATUS_SUCCESS;
}

//nwayacd group_number
SWITCH_STANDARD_APP(nwayacd_function){
	char *group_number = NULL;
	acd_t *nway_acd=NULL;
	char* black_ring =NULL;
	char* transfer_ring = NULL;
	char* busy_ring = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	if (!zstr(data)) {
		group_number = switch_core_session_strdup(session, data);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "group number:%s\n",group_number);
	}else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No Destination number provided \n");
		goto end;
	}
	if (!(nway_acd = (acd_t*)switch_core_session_alloc(session,sizeof(acd_t)))){
		goto end;
	}
	black_ring = (char*)switch_core_session_alloc(session,255);
	transfer_ring = (char*)switch_core_session_alloc(session,255);
	busy_ring=(char*)switch_core_session_alloc(session,255);
	if (!black_ring || !transfer_ring || !busy_ring){
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No memory to use \n");
		goto end;
	}
	nway_acd->group_name = switch_core_strdup(globals.pool,group_number);	
	nway_acd->black_ring = switch_core_strdup(globals.pool,black_ring);
	nway_acd->busy_ring = switch_core_strdup(globals.pool,busy_ring);
	nway_acd->transfer_ring = switch_core_strdup(globals.pool,transfer_ring);
	nway_acd->play_state=0;

	switch_channel_set_private(channel, "nway_acd", nway_acd);
	if (!switch_channel_test_flag(channel, CF_ANSWERED)){
		switch_channel_answer(channel);
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
	//char *group_number = NULL;
	acd_t *nway_acd=NULL;
	char* black_ring =NULL;
	char* transfer_ring = NULL;
	char* busy_ring = NULL;
	switch_channel_t *channel = NULL;//switch_core_session_get_channel(session);
	/////////////////////////////////////////
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
		switch_channel_t *channel = switch_core_session_get_channel(rsession);

		if (!(nway_acd = (acd_t*)switch_core_session_alloc(rsession,sizeof(acd_t)))){
			goto done;
		}
		black_ring = (char*)switch_core_session_alloc(rsession,255);
		transfer_ring = (char*)switch_core_session_alloc(rsession,255);
		busy_ring=(char*)switch_core_session_alloc(rsession,255);
		if (!black_ring || !transfer_ring || !busy_ring){
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rsession), SWITCH_LOG_WARNING, "No memory to use \n");
			goto done;
		}
		nway_acd->group_name = switch_core_strdup(globals.pool,group_number);	
		nway_acd->black_ring = switch_core_strdup(globals.pool,black_ring);
		nway_acd->busy_ring = switch_core_strdup(globals.pool,busy_ring);
		nway_acd->transfer_ring = switch_core_strdup(globals.pool,transfer_ring);
		nway_acd->play_state=0;
		switch_channel_set_private(channel, "nway_acd", nway_acd);
		if (!switch_channel_test_flag(channel, CF_ANSWERED)){
			switch_channel_answer(channel);
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
	//启动线程
	globals.running = 1;
	queue_process_thread_start();
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
	globals.running = 0;
	switch_yield(12000000);
	if (PQstatus(globals.db_connection) == CONNECTION_OK) {
		PQfinish(globals.db_connection);
	}
	switch_mutex_destroy(globals.mutex);
	return SWITCH_STATUS_SUCCESS;
}

