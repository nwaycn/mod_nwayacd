# mod_nwayacd

#### 介绍
由于从2013年开始一直采用ESL对freeswitch中的排队和路由进行处理，现在想实现一个基于FreeSWITCH的座席排队的acd 模块，采用数据库postgresql维护相关关连的座席分机，座席组，在座席签入时，动态指定座席组，先检查黑名单号码表，非黑名单就按座席组的分配规则处理座席排队呼叫，在呼叫时，同时触发esl事件，告诉系统，谁处理了这一通来电；如果转接不成功，那么由模块自动的再分配座席，如果需要排队，那么可以配置vip号码，优先处理vip号码，然后实时检测空闲的座席，当有空闲座席时，再进一步按vip优先，普通在后，按时间先后顺序进行排队。

#### 软件架构
软件架构说明
当呼入进组时，由模块对座席进行查询排队、呼叫等等。

#### 安装教程

1. 需要安装postgresql-devel postgresql-libs等库

2. 需要先编译和安装freeswitch

3. 在freeswitch的application路径下把此仓库代码放置于源码路径下，

4. 配置Makefile中的postgresql路径，如：
   ```PQROOT=/usr/local/pgsql`

   `PQINCLUDE=-I${PQROOT}/include`
   PQLIBRARY=-L${PQROOT}/lib  ```

5. make ,然后cp mod_nwayacd.so /usr/local/freeswitch/mod/.下

#### 使用说明

1.  修改nwayacd.conf.xml中的数据库相关信息

2.  把db/nwaycc.sql导入到postgresql数据库中。

3.  在座席组中添加几个座席组，如 12345组，119组等

4.  在座席登录时，需要动态的在 ext_group_map 把登录的座席和座席组对应加进去

5.  需要在座席登录时，修改座席分机在 call_extension 表中的seat_state='up',seat_status='idle' ,call_state='ready';

6.  配置路由 <action appliction="nwayacd" data="12345"/>

7.  那么当有电话进来后，会自动按规定的模式寻找空闲的座席进行分配



#### 数据库说明

1.  call_extension    分机表

2.  call_blacklist    黑名单表

3.  call_vip_number   VIP来电表

4.  cf_call_remember  短期来电记忆表，按实际需要进行定期清理

5.  ext_group         座席组表

    COMMENT ON TABLE public.ext_group
        IS '分机座席分组，面向12345业务';

    COMMENT ON COLUMN public.ext_group.group_name IS '组或座席组名';

    COMMENT ON COLUMN public.ext_group.group_number IS '座席组短号';

    COMMENT ON COLUMN public.ext_group.current_ext_number IS '当前组里接听时找到的分机，如果找不到，则再从头开始';

    COMMENT ON COLUMN public.ext_group.group_call_mode IS '呼叫模式 0 顺序，1 随机，2 循环，3 记忆优先+ 0 ，4 记忆优先+1 ，5 记忆优先+2';

    COMMENT ON COLUMN public.ext_group.group_callout_timeout IS '呼叫到每个号码时的超时时长，默认15秒';

6.  ext_group_map     座席组和座席对应表，支持多对多

### API说明
1.  nwayacd uuid group

用于将某个uuid的呼叫按acd规则进行排队

2.  nway_login extension group_list(','分隔)

将某个座席分机绑到某几个组里  如将 1000 绑进 110,119 ,   nway_login 1000 110,119

3.  nway_logout extension

登录座席，清理该座席分机的绑定组关系

4.  nway_busy extension

对座席置忙

5.  nway_ready extension

对座席置闲

###APP说明
nwayacd group_number

将通话按acd转到某个空闲座席上


