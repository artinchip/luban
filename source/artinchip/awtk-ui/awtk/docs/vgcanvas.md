# AWTK 中的矢量图绘图函数

### 一、工作模式

强大的 Vector graphics library 对 GUI 来说至关重要，一些酷炫的效果更是离不开 Vector graphics library 的支持。最有名的 Vector graphics library 要数下面这些了：

* [GDI+](https://msdn.microsoft.com/en-us/library/ms533798(v=VS.85).aspx)
* [Quartz 2D](https://developer.apple.com/library/content/documentation/GraphicsImaging/Conceptual/drawingwithquartz2d/dq_overview/dq_overview.html)
* [skia](https://skia.org/)
* [cairo](https://www.cairographics.org/)
* [nanovg](https://github.com/memononen/nanovg)
* [agg](http://www.antigrain.com)
* [agge](https://github.com/tyoma/agge)

其中最后 5 个是开源的，skia 和 cairo 很强但也很大，不太适合嵌入式环境（可以用于 linux 嵌入式平台）。nanovg 是最简洁最优雅的，可惜它只支持 OpenGL，为此，我们用 agg/agge 作为 nanovg 软件渲染的后端。经过一番考虑之后，AWTK 的 canvas 根据硬件环境分为三个层次：

* 简约模式。支持基本的绘图函数，但不支持 Vector graphics 绘图函数，能实现 GUI 常见功能。适用于低端的硬件环境，如 CPU 主频小于 100M，RAM 不足以实现。

* 正常模式。支持最基本的绘图函数，和软件实现的 Vector graphics 绘图函数，由于软件实现的 Vector graphics 绘图函数性能不高，所以只有在必要的情况下才使用它们。此时采用 nanovg+agge 作为 Vector graphics library。嵌入式 linux 平台也可以选择 cairo 作为 Vector graphics library。

* GPU 模式。如果硬件支持 OpenGL，则使用 OpenGL 实现的 Vector graphics 绘图函数，并用它们包装成基本的绘图函数。此时采用 nanovg 作为 Vector graphics library。

> 注意：agg 使用 GPL 协议开源，如果在商业软件中使用，需要作者协商一下。

![](images/canvas.png)

### 二、接口函数

vgcanvas 的接口如何定义呢，nanovg 和 agg 的接口差异极大，必须为 AWTK 上层提供统一的接口。AWTK 采用了 [HTML5 canvas](http://www.w3school.com.cn/tags/html_ref_canvas.asp) 类似的接口，这套接口非常好用，但由于底层的 agg 和 nanovg 的限制，在有 GPU 和无 GPU 时，效果可能不太一样，甚至不兼容，所以在使用时尽量保守一点：)

```

ret_t vgcanvas_begin_path(vgcanvas_t* vg);
ret_t vgcanvas_move_to(vgcanvas_t* vg, float_t x, float_t y); 
ret_t vgcanvas_line_to(vgcanvas_t* vg, float_t x, float_t y); 
ret_t vgcanvas_quadratic_curve_to(vgcanvas_t* vg, float_t cpx, float_t cpy, float_t x, float_t y); 
ret_t vgcanvas_bezier_curve_to(vgcanvas_t* vg, float_t cp1x, float_t cp1y, float_t cp2x, float_t cp2y,
                               float_t x, float_t y); 
ret_t vgcanvas_arc_to(vgcanvas_t* vg, float_t x1, float_t y1, float_t x2, float_t y2, float_t r); 
ret_t vgcanvas_arc(vgcanvas_t* vg, float_t x, float_t y, float_t r, float_t start_angle,
                   float_t end_angle, bool_t ccw);
bool_t vgcanvas_is_point_in_path(vgcanvas_t* vg, float_t x, float_t y); 
ret_t vgcanvas_rect(vgcanvas_t* vg, float_t x, float_t y, float_t w, float_t h); 
ret_t vgcanvas_round_rect(vgcanvas_t* vg, float_t x, float_t y, float_t w, float_t h, float_t r); 
ret_t vgcanvas_ellipse(vgcanvas_t* vg, float_t x, float_t y, float_t rx, float_t ry);
ret_t vgcanvas_close_path(vgcanvas_t* vg);

...

```

### 三、使用示例

```
  vgcanvas_t* vg = canvas_get_vgcanvas(c);

  vgcanvas_set_line_width(vg, 2); 
  vgcanvas_set_fill_color(vg, color_init(0xff, 0xff, 0, 0xff));
  vgcanvas_set_stroke_color(vg, color_init(0, 0xff, 0, 0xff));
  vgcanvas_clear_rect(vg, 0, 0, vg->w, vg->h, color_init(0xf0, 0xf0, 0xf0, 0xff));
  
  vgcanvas_round_rect(vg, 10, 10, 100, 20, 5); 
  vgcanvas_stroke(vg);

  vgcanvas_rect(vg, 128, 10, 100, 20);
  vgcanvas_fill(vg);
```
