--------mod_nwayacd
--------nwaycc.sql
--------座席排队数据表
--------许可协议：Apache-2.0
--------开发团队：上海宁卫信息技术有限公司
--------联系Email:lihao@nway.com.cn
--------开始开发时间：2020-8-1 中华人民共和国建军节

--------分机表

CREATE SEQUENCE public.call_extension_id_seq
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 1
  CACHE 1;
ALTER TABLE public.call_extension_id_seq
  OWNER TO postgres;

CREATE TABLE public.call_extension
(
  id bigint NOT NULL DEFAULT nextval('call_extension_id_seq'::regclass),
  extension_name character varying(50) NOT NULL, -- 分机名称
  extension_number character varying(50) NOT NULL, -- 分机号码
  callout_number character varying(50) DEFAULT ''::character varying, -- 外呼时的号码
  extension_type bigint DEFAULT 0, -- 分机类型
  group_id bigint DEFAULT 0,
  extension_pswd character varying(130) DEFAULT 'nway.com.cn'::character varying,
  extension_login_state character varying(50) DEFAULT 'success'::character varying,
  extension_reg_state character varying(50) DEFAULT ''::character varying, -- 注册状态
  callout_gateway bigint DEFAULT 0, -- 出局网关
  is_allow_callout boolean DEFAULT true,
  is_record boolean DEFAULT false, -- 是否录音
  answer_without_state boolean DEFAULT false, -- 在不上线，不置闲的情况下就可以进行应答，默认是必须由客户端上线等
  say_job_number boolean DEFAULT false, -- 是否要报工号，不管是与否，当answerwithoutstate为true时，它都无效，报工号时，在本工号接听时，播放工号语音
  job_number character varying(40) DEFAULT ''::character varying, -- 工号，最大为40个字符串
  reg_state character varying(30) DEFAULT 'unreg'::character varying, -- REGED ,UNREG  ,分机发register的状态记录
  seat_state character varying(30) DEFAULT 'down'::character varying, -- up,down,座席上下线，可作为考勤的一部分
  seat_status character varying(30) DEFAULT 'idle'::character varying, -- idle,busy,座席忙闲，如上厕所，临时开会，不能用下线
  call_state character varying(30) DEFAULT 'ready'::character varying, -- READY,RING,TALKING,IVR
  lastupdatetime character varying(30),
  core_uuid character varying(300) DEFAULT ''::character varying,
  use_custom_value boolean DEFAULT false, -- 是否要客户满意度评价
  client_ip character varying(50) DEFAULT '127.0.0.1'::character varying, -- 客户端 ip，用于mircosip自动获取帐号和密码
  is_register boolean DEFAULT false,
  is_select boolean DEFAULT false, -- 是否被网关组选中
  locked boolean DEFAULT false, -- 是否被锁定，锁定对于外呼或呼入来说不应使用
  login_password character varying(50) DEFAULT 'nway.com.cn'::character varying,
  last_reg_time timestamp without time zone DEFAULT now(), -- 最后注册时间
  dnd boolean DEFAULT false, -- 免打扰
  unconditional_forward character varying(30) DEFAULT ''::character varying, -- 无条件转移，有号码就直接转到这个号码，如果为空就转分机
  call_level integer DEFAULT 0,
  follow_me character varying(30) DEFAULT ''::character varying,
  last_hangup_time timestamp without time zone DEFAULT now(),
  last_state_change_time timestamp without time zone DEFAULT now(),
  CONSTRAINT call_extension_pkey PRIMARY KEY (id)
)
WITH (
  OIDS=FALSE
);
ALTER TABLE public.call_extension
  OWNER TO postgres;
COMMENT ON COLUMN public.call_extension.extension_name IS '分机名称';
COMMENT ON COLUMN public.call_extension.extension_number IS '分机号码';
COMMENT ON COLUMN public.call_extension.callout_number IS '外呼时的号码';
COMMENT ON COLUMN public.call_extension.extension_type IS '分机类型';
COMMENT ON COLUMN public.call_extension.extension_reg_state IS '注册状态';
COMMENT ON COLUMN public.call_extension.callout_gateway IS '出局网关';
COMMENT ON COLUMN public.call_extension.is_record IS '是否录音';
COMMENT ON COLUMN public.call_extension.answer_without_state IS '在不上线，不置闲的情况下就可以进行应答，默认是必须由客户端上线等';
COMMENT ON COLUMN public.call_extension.say_job_number IS '是否要报工号，不管是与否，当answerwithoutstate为true时，它都无效，报工号时，在本工号接听时，播放工号语音';
COMMENT ON COLUMN public.call_extension.job_number IS '工号，最大为40个字符串';
COMMENT ON COLUMN public.call_extension.reg_state IS 'REGED ,UNREG  ,分机发register的状态记录';
COMMENT ON COLUMN public.call_extension.seat_state IS 'up,down,座席上下线，可作为考勤的一部分';
COMMENT ON COLUMN public.call_extension.seat_status IS 'idle,busy,座席忙闲，如上厕所，临时开会，不能用下线';
COMMENT ON COLUMN public.call_extension.call_state IS 'READY,RING,TALKING,IVR';
COMMENT ON COLUMN public.call_extension.use_custom_value IS '是否要客户满意度评价';
COMMENT ON COLUMN public.call_extension.client_ip IS '客户端 ip，用于mircosip自动获取帐号和密码';
COMMENT ON COLUMN public.call_extension.is_select IS '是否被网关组选中';
COMMENT ON COLUMN public.call_extension.locked IS '是否被锁定，锁定对于外呼或呼入来说不应使用';
COMMENT ON COLUMN public.call_extension.last_reg_time IS '最后注册时间';
COMMENT ON COLUMN public.call_extension.dnd IS '免打扰';
COMMENT ON COLUMN public.call_extension.unconditional_forward IS '无条件转移，有号码就直接转到这个号码，如果为空就转分机';


-- Index: public."IDX_CALL_EXTENSION_EXT_NUMBER"

-- DROP INDEX public."IDX_CALL_EXTENSION_EXT_NUMBER";

CREATE UNIQUE INDEX "IDX_CALL_EXTENSION_EXT_NUMBER"
  ON public.call_extension
  USING btree
  (extension_number COLLATE pg_catalog."default");


---------黑名单表

CREATE SEQUENCE public.call_blacklist_id_seq
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 4
  CACHE 1;
ALTER TABLE public.call_blacklist_id_seq
  OWNER TO postgres;
CREATE TABLE public.call_blacklist
(
  id bigint NOT NULL DEFAULT nextval('call_blacklist_id_seq'::regclass),
  category integer DEFAULT 0, -- 类型，全局黑名单0，对外黑名单1，呼入黑名单2
  call_number character varying(30) NOT NULL,
  dest_number character varying(30) DEFAULT ''::character varying,
  group_number character varying(50),
  CONSTRAINT "PK_BLACKLIST_NUMBER" PRIMARY KEY (call_number)
)
WITH (
  OIDS=FALSE
);
ALTER TABLE public.call_blacklist
  OWNER TO postgres;
COMMENT ON COLUMN public.call_blacklist.category IS '类型，全局黑名单0，对外黑名单1，呼入黑名单2';


-- Index: public."BLACKLIST_NUMBER_IDX"

-- DROP INDEX public."BLACKLIST_NUMBER_IDX";

CREATE INDEX "BLACKLIST_NUMBER_IDX"
  ON public.call_blacklist
  USING btree
  (call_number COLLATE pg_catalog."default", category);

---------vip表
CREATE SEQUENCE public.call_vip_number_id_seq
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 1
  CACHE 1;
ALTER TABLE public.call_vip_number_id_seq
  OWNER TO postgres;




CREATE TABLE public.call_vip_number
(
  id bigint NOT NULL DEFAULT nextval('call_vip_number_id_seq'::regclass),
  phone_number character varying(50), -- 号码
  group_number character varying(50) -- 属于哪个组
)
WITH (
  OIDS=FALSE
);
ALTER TABLE public.call_vip_number
  OWNER TO postgres;
COMMENT ON COLUMN public.call_vip_number.phone_number IS '号码';
COMMENT ON COLUMN public.call_vip_number.group_number IS '属于哪个组';

---------记忆呼转缓存表
CREATE SEQUENCE public.cf_call_remember_id_seq
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 1
  CACHE 1;
ALTER TABLE public.cf_call_remember_id_seq
  OWNER TO postgres;



CREATE TABLE public.cf_call_remember
(
  id bigint NOT NULL DEFAULT nextval('cf_call_remember_id_seq'::regclass),
  call_number character varying(50), -- 呼入号码
  agent_number character varying(50), -- 座席号码
  insert_time timestamp without time zone DEFAULT now(), -- 插入时间
  group_number character varying(50) -- 组
)
WITH (
  OIDS=FALSE
);
ALTER TABLE public.cf_call_remember
  OWNER TO postgres;
COMMENT ON COLUMN public.cf_call_remember.call_number IS '呼入号码 ';
COMMENT ON COLUMN public.cf_call_remember.agent_number IS '座席号码';
COMMENT ON COLUMN public.cf_call_remember.insert_time IS '插入时间';
COMMENT ON COLUMN public.cf_call_remember.group_number IS '组';


-- Index: public."IDX_CALL_REMEMBER_NUMBER"

-- DROP INDEX public."IDX_CALL_REMEMBER_NUMBER";

CREATE INDEX "IDX_CALL_REMEMBER_NUMBER"
  ON public.cf_call_remember
  USING btree
  (call_number COLLATE pg_catalog."default", group_number COLLATE pg_catalog."default");


---------座席组表
 CREATE SEQUENCE public.ext_group_id_seq
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 1
  CACHE 1;
ALTER TABLE public.ext_group_id_seq
  OWNER TO postgres;



CREATE SEQUENCE public.ext_group_map_id_seq
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 1
  CACHE 1;
ALTER TABLE public.ext_group_map_id_seq
  OWNER TO postgres;


--固定座席组

CREATE TABLE public.ext_group
(
  id bigint NOT NULL DEFAULT nextval('ext_group_id_seq'::regclass),
  group_name character varying(100), -- 组或座席组名
  group_number character varying(50), -- 座席组短号
  current_ext_number character varying(50) ,-- 当前组里接听时找到的分机，如果找不到，则再从头开始
  group_call_mode integer DEFAULT 5 ,-- 呼叫模式  ...
  group_callout_timeout integer DEFAULT 15 -- 呼叫到每个号码时的超时时长，默认15秒
)
WITH (
  OIDS=FALSE
);
ALTER TABLE public.ext_group
  OWNER TO postgres;
COMMENT ON TABLE public.ext_group
  IS '分机座席分组，面向12345业务';
COMMENT ON COLUMN public.ext_group.group_name IS '组或座席组名';
COMMENT ON COLUMN public.ext_group.group_number IS '座席组短号';
COMMENT ON COLUMN public.ext_group.current_ext_number IS '当前组里接听时找到的分机，如果找不到，则再从头开始';
COMMENT ON COLUMN public.ext_group.group_call_mode IS '呼叫模式 0 顺序，1 随机，2 循环，3 记忆优先+ 0 ，4 记忆优先+1 ，5 记忆优先+2';
COMMENT ON COLUMN public.ext_group.group_callout_timeout IS '呼叫到每个号码时的超时时长，默认15秒';
--动态座席组和座席对应表

CREATE TABLE public.ext_group_map
(
  id bigint NOT NULL DEFAULT nextval('ext_group_map_id_seq'::regclass),
  ext_group_id bigint, -- 分机组id
  ext_group_number character varying(50), -- 座席组号码
  ext character varying(50) -- 分机
)
WITH (
  OIDS=FALSE
);
ALTER TABLE public.ext_group_map
  OWNER TO postgres;
COMMENT ON COLUMN public.ext_group_map.ext_group_id IS '分机组id';
COMMENT ON COLUMN public.ext_group_map.ext_group_number IS '座席组号码';
COMMENT ON COLUMN public.ext_group_map.ext IS '分机';



----------------------------------------------------------------------
----排队
----------------------------------------------------------------------
CREATE SEQUENCE public.callin_queue_id_seq
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 1
  CACHE 1;
ALTER TABLE public.callin_queue_id_seq
  OWNER TO postgres;
CREATE TABLE public.callin_queue
(
  id bigint NOT NULL DEFAULT nextval('callin_queue_id_seq'::regclass),
  callin_number character varying(50), -- 呼入号码
  callin_group character varying(50), -- 呼入组
  callin_type integer DEFAULT 0, -- 0为普通，1为VIP
  call_time timestamp without time zone DEFAULT now() ,-- 开始排队时间
  call_uuid character varying(50) DEFAULT ''::character varying -- 一条独立的uuid,用来找到该呼叫
)
WITH (
  OIDS=FALSE
);
ALTER TABLE public.callin_queue
  OWNER TO postgres;
COMMENT ON TABLE public.callin_queue
  IS '呼入但座席忙的情况下，进行排队，排队则是先进先出';
COMMENT ON COLUMN public.callin_queue.callin_number IS '呼入号码';
COMMENT ON COLUMN public.callin_queue.callin_group IS '呼入组';
COMMENT ON COLUMN public.callin_queue.callin_type IS '0为普通，1为VIP';
COMMENT ON COLUMN public.callin_queue.call_time IS '开始排队时间';
COMMENT ON COLUMN public.callin_queue.call_uuid IS '一条独立的uuid,用来找到该呼叫';