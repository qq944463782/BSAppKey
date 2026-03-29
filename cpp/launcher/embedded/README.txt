发布前把真实文件放到本目录再编译壳，资源会打进 bl_launcher.exe：

  public.blb   — 与注册机生成的公钥一致（可覆盖仓库里的占位文件）
  payload.exe  — 你要保护/启动的真实程序

也可运行 cpp\prepare_embedded.bat 从常见路径自动复制（按需编辑脚本）。

若资源无效或缺失，运行时仍会尝试同目录下的 public.blb 与 payload.exe。
