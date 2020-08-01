# mod_nwayacd

#### 介绍
实现一个基于FreeSWITCH的座席排队的acd 模块，采用数据库postgresql维护相关关连的座席分机，座席组，在座席签入时，动态指定座席组，先检查黑名单号码表，非黑名单就按座席组的分配规则处理座席排队呼叫，在呼叫时，同时触发esl事件，告诉系统，谁处理了这一通来电；如果转接不成功，那么由模块自动的再分配座席，如果需要排队，那么可以配置vip号码，优先处理vip号码，然后实时检测空闲的座席，当有空闲座席时，再进一步按vip优先，普通在后，按时间先后顺序进行排队。

#### 软件架构
软件架构说明


#### 安装教程

1.  xxxx
2.  xxxx
3.  xxxx

#### 使用说明

1.  xxxx
2.  xxxx
3.  xxxx

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 码云特技

1.  使用 Readme\_XXX.md 来支持不同的语言，例如 Readme\_en.md, Readme\_zh.md
2.  码云官方博客 [blog.gitee.com](https://blog.gitee.com)
3.  你可以 [https://gitee.com/explore](https://gitee.com/explore) 这个地址来了解码云上的优秀开源项目
4.  [GVP](https://gitee.com/gvp) 全称是码云最有价值开源项目，是码云综合评定出的优秀开源项目
5.  码云官方提供的使用手册 [https://gitee.com/help](https://gitee.com/help)
6.  码云封面人物是一档用来展示码云会员风采的栏目 [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
