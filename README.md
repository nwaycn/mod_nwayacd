# mod_nwayacd

#### 介绍
由于从2013年开始一直采用ESL对freeswitch中的排队和路由进行处理，现在想实现一个基于FreeSWITCH的座席排队的acd 模块，采用数据库postgresql维护相关关连的座席分机，座席组，在座席签入时，动态指定座席组，先检查黑名单号码表，非黑名单就按座席组的分配规则处理座席排队呼叫，在呼叫时，同时触发esl事件，告诉系统，谁处理了这一通来电；如果转接不成功，那么由模块自动的再分配座席，如果需要排队，那么可以配置vip号码，优先处理vip号码，然后实时检测空闲的座席，当有空闲座席时，再进一步按vip优先，普通在后，按时间先后顺序进行排队。

#### 软件架构
软件架构说明


#### 安装教程

1.  需要安装postgresql-devel postgresql-libs等库
2.  需要先编译和安装freeswitch
3.  在freeswitch的application路径下把此仓库代码放置于源码路径下，make ,然后cp mod_nwayacd /usr/local/freeswitch/mod/.下

#### 使用说明

1.  修改nwayacd.conf.xml中的数据库相关信息
2.  配置路由
3.  呼入后进入路由即可

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


 
