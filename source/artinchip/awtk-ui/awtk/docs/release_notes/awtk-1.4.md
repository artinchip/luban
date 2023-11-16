
## ZLG AWTK 1.4 Release Notes

## 一、介绍

[AWTK](README.md) 全称 Toolkit AnyWhere，是 [ZLG](http://www.zlg.cn/) 开发的开源 GUI 引擎，旨在为嵌入式系统、WEB、各种小程序、手机和 PC 打造的通用 GUI 引擎，为用户提供一个功能强大、高效可靠、简单易用、可轻松做出炫酷效果的 GUI 引擎。

> 欢迎广大开发者一起参与开发：[生态共建计划](docs/awtk_ecology.md)。

#### [AWTK](README.md) 寓意有两个方面：

* Toolkit AnyWhere。 
* ZLG 物联网操作系统 AWorksOS 内置 GUI。

#### [AWTK](README.md) 源码仓库：

* 主源码仓库：[https://github.com/zlgopen/awtk](https://github.com/zlgopen/awtk)
* 镜像源码仓库：[https://gitee.com/zlgopen/awtk](https://gitee.com/zlgopen/awtk)
* 稳定版整合包：https://pan.baidu.com/s/1_oRgj67M-I4kivk-YzwFWA   提取码：1cmi

#### AWTK Designer 界面设计工具：

* 不再需要手写 XML
* 拖拽方式设计界面，所见即所得
* 快速预览，一键打包资源
* 注册及下载地址：https://awtk.zlg.cn

![AWTK Designer](../images/designer.png)

#### 运行效果截图：

![Chart-Demo](../images/chart_main.png)

![MusicPlayer-Demo](../images/musicplayer_main.png)

![Watch](../images/smartwatch_main.png)

## 二、最终目标：

* 支持开发嵌入式应用程序。✔
* 支持开发 Linux 应用程序。✔
* 支持开发 MacOS 应用程序。✔
* 支持开发 Windows 应用程序。✔
* 支持开发 Web APP。✔
* 支持开发 Android 应用程序。✔
* 支持开发 iOS 应用程序。✔
* 支持开发微信小程序。
* 支持开发支付宝小程序。
* 支持开发百度小程序。
* 支持开发 2D 小游戏。

## 三、主要特色

### 1. 跨平台

[AWTK](README.md) 是跨平台的，这有两个方面的意思：

* AWTK 本身是跨平台的。目前支持的平台有 ZLG AWorksOS、Windows、Linux、MacOS、嵌入式 Linux、Android、iOS、Web 和嵌入式裸系统，可以轻松的移植到各种 RTOS 上。AWTK 以后也可以运行在各种小程序平台上运行。

* AWTK 同时还提供了一套跨平台的基础工具库。其中包括链表、数组、字符串 (UTF8 和 widechar)，事件发射器、值、对象、文件系统、互斥锁和线程、表达式和字符串解析等等，让你用 AWTK 开发的应用程序可以真正跨平台运行。

### 2. 高效

[AWTK](README.md) 通过一系列的手段保证 AWTK 应用程序高效运行：

* 通过脏矩算法只更新变化的部分。
* 支持 3 FrameBuffer 让界面以最高帧率运行 （可选）。
* UI 描述文件和窗体样式文件使用高效的二进制格式，解析在瞬间完成。
* 支持各种 GPU 加速接口。如 OpenGL、DirectX、Vulkan 和 Metal 等。
* 支持嵌入式平台的各种 2D 加速接口。目前 STM32 的 DMA2D 和 NXP 的 PXP 接口，厂家可以轻松扩展自己的加速接口。

### 3. 稳定

[AWTK](README.md) 通过下列方式极力让代码稳定可靠：

* 使用 cppcheck 和 facebook infer 进行静态检查。
* 使用 valgrind 进行动态内存检查。
* 近两万行的单元测试代码。
* ZLG 强大 GUI 团队的支持。
* 经过多个实际项目验证。
* 多平台 / 多编译器验证。
* 优秀的架构设计。
* Code Review。
* 手工测试。

### 4. 强大

* 丰富的控件 （持续增加中）。
* 支持各种图片格式 (png/jpg/gif/svg)。
* 支持各种字体格式 （点阵和矢量）。
* 支持窗口动画
* 支持控件动画
* 支持高清屏。
* 支持界面描述文件。
* 支持窗体样式描述文件。
* 主题切换实时生效。
* 支持控件布局策略。
* 支持对话框高亮策略。
* 丰富的辅助工具。
* 支持从低端的 Cortex M3 到各种高端 CPU。
* 支持无文件系统和自定义的文件系统。
* 支持裸系统和 RTOS。

### 5. 易用

* 大量的示例代码。
* 完善的 API 文档和使用文档。
* ZLG 强大的技术支持团队。
* 用 AWTK 本身开发的 [界面编辑器](https://awtk.zlg.cn)。
* 声明式的界面描述语言。一行代码启用控件动画，启用窗口动画，显示图片 (png/jpg/svg/gif)。

### 6. 高度扩展性

* 可以扩展自己的控件。
* 可以扩展自己的动画。
* 可以实现自己的主循环。
* 可以扩展自己的软键盘。
* 可以扩展自己的图片加载器。
* 可以扩展自己的字体加载器。
* 可以扩展自己的输入法引擎。
* 可以扩展自己的控件布局算法。
* 可以扩展自己的对话框高亮策略。
* 可以实现自己的 LCD 接口。
* 可以扩展自己的矢量引擎 （如使用 skia/cairo)。
* 所有扩展组件和内置组件具有相同的待遇。

### 7. 多种开发语言

[AWTK](README.md) 本身是用 C 语言开发的，可以通过 IDL 生成各种脚本语言的绑定。生成的绑定代码不是简单的把 C 语言的 API 映射到脚本语言，而是生成脚本语言原生代码风格的 API。目前支持以下语言 （以后根据需要增加）：

* C
* C++
* lua
* java
* python
* Javascript on jerryscript
* Javascript on nodejs
* Javascript on quickjs

### 8. 国际化

* 支持 Unicode。
* 支持输入法。
* 支持字符串翻译 （实时生效）。
* 支持图片翻译 （实时生效）。
* 文字双向排版 （计划中）。

### 9. 为嵌入式软件定制的 MVVM 框架，彻底分离用户界面和业务逻辑。
* 性能高。
* 内存开销小。
* 隔离更彻底。
* 可移植到其它 GUI。
* 代码小 (~5000 行）。
* 无需学习 AWTK 控件本身的 API。
* 支持多种编程语言（目前支持 C/JS)。

> 详情请参考：https://github.com/zlgopen/awtk-mvvm

### 10. 开放源码，免费商用 (LGPL)。

## 四、1.4 版本更新
-------------------

### 1. 细节完善
  * 完善 fs 接口。
  * 完善工具支持多主题。
  * list view 支持上下键滚动。
  * 完善窗口切换时焦点恢复的问题。
  * 完善 combobox，选择之后重新打开输入法。
  * progress circle 支持 line cap 属性。
  * 增加 vgcanvas\_line\_join\_t 定义。
  * 增加 vgcanvas\_line\_cap\_t 定义。
  * 修改 android resume 后界面黑屏的问题。
  * slide view/pages 每个页面支持独立的初始焦点。
  * 增加函数 widget\_set\_child\_text\_utf8。
  * 增加函数 widget\_set\_child\_text\_with\_double。
  * keyboard 在 grab_keys 时，keyboard 处理 key 事件后，应用窗口不再处理。
  * 完善 image value，支持点击时加上一个增量，增加到最大值后回到最小值。

> 大量细节完善去请参考： https://github.com/zlgopen/awtk/blob/master/docs/changes.md

### 3. 新增特性
  * 无文件系统是支持多主题。
  * opengles 支持 snapshot。
  * dit/mledit 支持自己指定软键盘名称。
  * 点击鼠标右键触发 context menu 事件。
  * 使用 event\_source\_manager 实现主循环。
  * 增加 awtk\_main.inc，用于标准程序的主函数。
  * 用 SDL 重新实现 PC 版本的线程和同步相关函数 。
  * edit 增加 input type "custom_password"类型。

### 4. 新增控件
  * audio_view [音频播放控件](https://github.com/zlgopen/awtk-media-player)。
  * lrc_view [歌词显示控件](https://github.com/zlgopen/awtk-media-player)。
  * video_view [视频播放控件](https://github.com/zlgopen/awtk-media-player)。

### 5. 新增重要 API
  * 增加 action thread。
  * 增加 action thread pool。
  * 增加动态链接库的接口 dl.h。
  * 增加 waitable ring buffer。
  * 增加 widget\_close\_window。
  * 增加 waitable\_action\_queue。 
  * 增加 path\_replace\_extname 函数。
  * 增加 async.c/.h 用于实现函数异步调用。
  * 增加 path\_replace\_extname 函数。
  * 增加 async.c/.h 用于实现函数异步调用。
  * 增加 data reader 接口和 data writer，用于抽象外部 flash 等设备。
  * 增加函数 fs\_get\_user\_storage\_path 用于统一 PC 和 android 平台保存数据的目录。

### 6. 新增平台
  * [ios](https://github.com/zlgopen/awtk-ios)

### 7. 新增语言绑定
  * [python](https://github.com/zlgopen/awtk-python)
  * [java](https://github.com/zlgopen/awtk-java)
  * [cpp](https://github.com/zlgopen/awtk-cpp)
  * [nodejs](https://github.com/zlgopen/awtk-nodejs)
  * [minijvm](https://github.com/zlgopen/awtk-minijvm)

### 8. 新增相关项目
  * [http client](https://github.com/zlgopen/awtk-http-client/)
  * [media player](https://github.com/zlgopen/awtk-media-player/)
  * [awtk-mobile-plugins(android 平台插件）](https://github.com/zlgopen/awtk-mobile-plugins/)

> 欢迎广大开发者一起参与开发：[生态共建计划](../awtk_ecology.md)。
