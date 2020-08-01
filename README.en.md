# mod_nwayacd

#### Description
实现一个基于FreeSWITCH的座席排队的acd 模块，采用数据库postgresql维护相关关连的座席分机，座席组，在座席签入时，动态指定座席组，先检查黑名单号码表，非黑名单就按座席组的分配规则处理座席排队呼叫，在呼叫时，同时触发esl事件，告诉系统，谁处理了这一通来电；如果转接不成功，那么由模块自动的再分配座席，如果需要排队，那么可以配置vip号码，优先处理vip号码，然后实时检测空闲的座席，当有空闲座席时，再进一步按vip优先，普通在后，按时间先后顺序进行排队。

#### Software Architecture
Software architecture description

#### Installation

1.  xxxx
2.  xxxx
3.  xxxx

#### Instructions

1.  xxxx
2.  xxxx
3.  xxxx

#### Contribution

1.  Fork the repository
2.  Create Feat_xxx branch
3.  Commit your code
4.  Create Pull Request


#### Gitee Feature

1.  You can use Readme\_XXX.md to support different languages, such as Readme\_en.md, Readme\_zh.md
2.  Gitee blog [blog.gitee.com](https://blog.gitee.com)
3.  Explore open source project [https://gitee.com/explore](https://gitee.com/explore)
4.  The most valuable open source project [GVP](https://gitee.com/gvp)
5.  The manual of Gitee [https://gitee.com/help](https://gitee.com/help)
6.  The most popular members  [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
