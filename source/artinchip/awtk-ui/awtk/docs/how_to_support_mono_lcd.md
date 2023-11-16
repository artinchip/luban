# 支持单色 LCD

## lcd_mono 实现

src/lcd/lcd_mono.c 提供单色 LCD 的 framebuffer（每个像素占用一个比特），实现了 LCD 的基本绘制函数，在使用时调用者需要提供 flush 函数，负责把脏矩形内的像素提交到 LCD 硬件。

## 模拟器

在 awtk_config.py 中可以打开单色 LCD 的配置。

```
LCD='SDL_FB_MONO'
```

## 单色字体生成

字体生成工具 fontgen 支持生成单色字体，加上可选参数 mono 即可。

```
fontgen.exe ttf_filename str_filename out_filename font_size [mono]
```

## 单色图片生成

图片生成工具 imagegen 支持生成单色图片，加上可选参数 mono 即可。

```
imagegen.exe in_filename out_filename (bgra|bgr565|mono)
```

> 由于单色图片只有两种颜色，所以在设计图片上只能使用白色 (#ffffff) 和黑色 (#000000)，否则转换出来的效果不是期望的。

## 窗体样式配置

窗体样式中只能使用白色 (#ffffff) 和黑色 (#000000) 两种颜色。

## 资源生成

生成资源时需要加上 mono 参数，该参数会被传递给 fontgen 和 imagegen。

```
python scripts/update_res.py all x1 mono
```

## 应用程序

应用程序 UI 设计请考虑单色 LCD 的特殊性。

> demoui 没有考虑单色 LCD，在单色 LCD 上运行效果不佳。

## 预览 mono test 查看运行效果

```
./bin/preview_ui design/default/ui/mono_test.xml 
```

## 其它

* 嵌入式系统，请在 awtk\_config.h 中定义下面的宏。

```
#define WITH_LCD_MONO 1
```

* awtk-linux-fb，请在 awtk\_config.py 中定义下面的宏。

```
COMMON_CCFLAGS = COMMON_CCFLAGS + ' -DWITH_LCD_MONO=1 '
```

## 参考：

* [免费像素字体](https://www.uisdc.com/25-pixel-fonts)

* [Unifont 点阵黑：一款超大字符集免费商用像素字体](https://www.maoken.com/freefonts/3747.html)
