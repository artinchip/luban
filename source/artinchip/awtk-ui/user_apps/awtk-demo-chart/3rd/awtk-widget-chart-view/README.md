# chart_view

chart_view图表控件，包含：曲线图、柱状图和饼图。源码位于src目录，示例位于demos目录。

- ##### 控件列表

|       控件类型       |               说明               |
| :------------------: | :------------------------------: |
|        x_axis        |             X轴控件              |
|        y_axis        |             Y轴控件              |
|       tooltip        |          提示信息框控件          |
|     line_series      |            曲线图控件            |
| line_series_colorful |          彩色曲线图控件          |
|      bar_series      |            柱状图控件            |
|  bar_series_minmax   | 柱状图控件（同时显示最大最小值） |
|      pie_slice       |             饼图控件             |

- ##### 运行示例效果截图

![](docs\images\曲线图.png)



![](docs\images\柱状图.png)



![](docs\images\饼图.png)

## 准备

1. 获取 awtk 并编译

```
git clone https://github.com/zlgopen/awtk.git
cd awtk; scons; cd -
```

## 运行

1. 生成示例代码的资源

```
python scripts/update_res.py all
```
> 也可以使用 Designer 打开项目，之后点击 “打包” 按钮进行生成；
> 如果资源发生修改，则需要重新生成资源。

如果 PIL 没有安装，执行上述脚本可能会出现如下错误：
```cmd
Traceback (most recent call last):
...
ModuleNotFoundError: No module named 'PIL'
```
请用 pip 安装：
```cmd
pip install Pillow
```

2. 编译

```
scons
```
> 注意：
> 编译前先确认 SConstruct 文件中的 awtk_root 是否为 awtk 所在目录，不是则修改。
> 默认使用动态库的形式，如果需要使用静态库，修改 SConstruct 文件中的 BUILD_SHARED = 'false' 即可。

3. 运行
```
./bin/demo
```


