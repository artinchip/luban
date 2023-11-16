## widget\_state\_t
### 概述
控件状态常量定义。

这里指定常用的状态值，扩展控件可以在自己的头文件中定义私有的状态。
### 常量
<p id="widget_state_t_consts">

| 名称 | 说明 | 
| -------- | ------- | 
| WIDGET\_STATE\_NONE | 无效状态。 |
| WIDGET\_STATE\_NORMAL | 正常状态。 |
| WIDGET\_STATE\_ACTIVATED | 3/5keys模式时，进入激活状态(此时方向键用于修改值)。 |
| WIDGET\_STATE\_CHANGED | 内容被修改的状态。 |
| WIDGET\_STATE\_PRESSED | 指针按下状态。 |
| WIDGET\_STATE\_OVER | 指针悬浮状态。 |
| WIDGET\_STATE\_DISABLE | 禁用状态。 |
| WIDGET\_STATE\_FOCUSED | 聚焦状态。 |
| WIDGET\_STATE\_CHECKED | 勾选状态。 |
| WIDGET\_STATE\_UNCHECKED | 没勾选状态。 |
| WIDGET\_STATE\_EMPTY | 编辑器无内容状态。 |
| WIDGET\_STATE\_EMPTY\_FOCUS | 编辑器无内容同时聚焦的状态。 |
| WIDGET\_STATE\_EMPTY\_OVER | 编辑器无内容同时指针悬浮的状态。 |
| WIDGET\_STATE\_ERROR | 输入错误状态。 |
| WIDGET\_STATE\_SELECTED | 选中状态。 |
| WIDGET\_STATE\_NORMAL\_OF\_CHECKED | 正常状态(选中项)。 |
| WIDGET\_STATE\_PRESSED\_OF\_CHECKED | 指针按下状态(选中项)。 |
| WIDGET\_STATE\_OVER\_OF\_CHECKED | 指针悬浮状态(选中项)。 |
| WIDGET\_STATE\_DISABLE\_OF\_CHECKED | 禁用状态(选中项)。 |
| WIDGET\_STATE\_FOCUSED\_OF\_CHECKED | 焦点状态(选中项)。 |
| WIDGET\_STATE\_NORMAL\_OF\_ACTIVE | 正常状态(当前项)。 |
| WIDGET\_STATE\_PRESSED\_OF\_ACTIVE | 指针按下状态(当前项)。 |
| WIDGET\_STATE\_OVER\_OF\_ACTIVE | 指针悬浮状态(当前项)。 |
| WIDGET\_STATE\_DISABLE\_OF\_ACTIVE | 禁用状态(当前项)。 |
| WIDGET\_STATE\_FOCUSED\_OF\_ACTIVE | 焦点状态(当前项)。 |
