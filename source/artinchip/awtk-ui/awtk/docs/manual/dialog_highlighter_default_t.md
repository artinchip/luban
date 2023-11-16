## dialog\_highlighter\_default\_t
### 概述
缺省对话框高亮策略。

>本策略在背景上画一层半透明的蒙版来高亮(突出)对话框本身。
>对于性能不高的平台，建议将start\_alpha和end\_alpha设为相同。
----------------------------------
### 函数
<p id="dialog_highlighter_default_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#dialog_highlighter_default_t_dialog_highlighter_default_create">dialog\_highlighter\_default\_create</a> | 创建缺省的对话框高亮策略。 |
### 属性
<p id="dialog_highlighter_default_t_properties">

| 属性名称 | 类型 | 说明 | 
| -------- | ----- | ------------ | 
| <a href="#dialog_highlighter_default_t_end_alpha">end\_alpha</a> | uint8\_t | 结束alpha，打开对话框的动画结束(直到对话框被关闭)时的alpha值。 |
| <a href="#dialog_highlighter_default_t_start_alpha">start\_alpha</a> | uint8\_t | 起始alpha，打开对话框的动画开始时的alpha值。 |
| <a href="#dialog_highlighter_default_t_system_bar_alpha">system\_bar\_alpha</a> | uint8\_t | 由于在没有过度动画的情况下，截图中已经包括黑色色块，为了让 system_bar 也同步高亮部分的色块透明。 |
| <a href="#dialog_highlighter_default_t_system_bar_bottom_clip_rects">system\_bar\_bottom\_clip\_rects</a> | darray\_t | 截图的底部 system_bar 显示裁减区列表 |
| <a href="#dialog_highlighter_default_t_system_bar_top_clip_rects">system\_bar\_top\_clip\_rects</a> | darray\_t | 截图的顶部 system_bar 显示裁减区列表 |
| <a href="#dialog_highlighter_default_t_update_background">update\_background</a> | bool\_t | 是否刷新底层窗口的截图。 |
| <a href="#dialog_highlighter_default_t_win_mask_rect_list">win\_mask\_rect\_list</a> | slist\_t | 窗口 mask 区域 |
#### dialog\_highlighter\_default\_create 函数
-----------------------

* 函数功能：

> <p id="dialog_highlighter_default_t_dialog_highlighter_default_create">创建缺省的对话框高亮策略。

* 函数原型：

```
dialog_highlighter_t* dialog_highlighter_default_create (tk_object_t* args);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | dialog\_highlighter\_t* | 返回对话框高亮策略对象。 |
| args | tk\_object\_t* | 参数。 |
#### end\_alpha 属性
-----------------------
> <p id="dialog_highlighter_default_t_end_alpha">结束alpha，打开对话框的动画结束(直到对话框被关闭)时的alpha值。

* 类型：uint8\_t

#### start\_alpha 属性
-----------------------
> <p id="dialog_highlighter_default_t_start_alpha">起始alpha，打开对话框的动画开始时的alpha值。

* 类型：uint8\_t

#### system\_bar\_alpha 属性
-----------------------
> <p id="dialog_highlighter_default_t_system_bar_alpha">由于在没有过度动画的情况下，截图中已经包括黑色色块，为了让 system_bar 也同步高亮部分的色块透明。

* 类型：uint8\_t

#### system\_bar\_bottom\_clip\_rects 属性
-----------------------
> <p id="dialog_highlighter_default_t_system_bar_bottom_clip_rects">截图的底部 system_bar 显示裁减区列表

* 类型：darray\_t

#### system\_bar\_top\_clip\_rects 属性
-----------------------
> <p id="dialog_highlighter_default_t_system_bar_top_clip_rects">截图的顶部 system_bar 显示裁减区列表

* 类型：darray\_t

#### update\_background 属性
-----------------------
> <p id="dialog_highlighter_default_t_update_background">是否刷新底层窗口的截图。

* 类型：bool\_t

#### win\_mask\_rect\_list 属性
-----------------------
> <p id="dialog_highlighter_default_t_win_mask_rect_list">窗口 mask 区域

* 类型：slist\_t

