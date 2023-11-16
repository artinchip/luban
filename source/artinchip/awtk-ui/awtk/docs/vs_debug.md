## 在 Visual Studio 中调试 AWTK

scons 编译时并没有生成 Visual Studio 的工程，如果需要个在 Visual Studio 中调试 AWTK 应用程序，可按下列步骤进行：

* 1. 打开 Visual Studio。
* 2. 在『文件』菜单中点击『打开』并选中『项目』。
* 3. 选择 awtk\bin\demoui.exe （或者其它要调试的可执行文件）。
* 4. 在项目设置中设置调试参数（可选）。
* 5. 进行调试。

参考：
[How to: Debug an Executable Not Part of a Visual Studio Solution](https://msdn.microsoft.com/en-us/library/0bxe8ytt.aspx?f=255&MSPPError=-2147217396)

> 建议使用 vscode 调试。