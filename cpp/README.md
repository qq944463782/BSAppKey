# C/C++ 授权壳（Win32 + CNG）

三语注册机说明：**`docs/注册机使用说明.md`**（简体 / 繁体 / English）。

## 推荐：图形界面加壳工具（你要的流程）

1. **Release x64** 先编译 **BlLauncher**（会顺带在同目录生成 **`shell_template.exe`** 壳模板）。再编译 **BlPacker**。
2. 在 **`x64\Release\`**（或你的输出目录）运行 **`bl_packer.exe`**（与 `shell_template.exe` 同目录）。
3. 在加壳工具里：**浏览** 你的原始已编译 **`*.exe`** → 浏览 **`public.blb`**（注册机生成的公钥）→ 指定 **输出路径** → **一键生成**。（加壳工具会从原始 exe **复制图标资源** 到输出文件，资源管理器里一般显示为你的程序图标。）
4. 把生成的 **单个 exe** 发给客户；客户运行 → 弹窗 **机器码** → 发你 → 你用 **`bl_keygen.exe`** 生成授权码发回。

> **不要**把 `private.blb`、注册机、加壳工具发给客户。

---

本目录还提供：

- **`bl_launcher.exe`**：壳本体（运行时）；也可被加壳工具当作 **`shell_template.exe`** 模板写入资源。
- **`bl_keygen.exe`**：注册机（仅你方），读取 **`private.blb`**，按机器码生成 Base64 授权码。

> 说明：当前为 **启动器 + RSA 离线授权**，不是完整 VMP；加壳工具把客户程序写入资源，客户只需一个 exe（公钥一并写入）。

### 加壳工具找不到模板？

模板 **`shell_template.exe`** 须与 **`bl_packer.exe` 在同一目录。

- **生成 BlLauncher（Debug 或 Release 均可）后**，会自动执行：`copy bl_launcher.exe → shell_template.exe`（与 exe 同一输出目录）。  
- 若没有生成：在 **`cpp`** 目录双击运行 **`copy_shell_template.bat`**，或手动 **`copy /Y bl_launcher.exe shell_template.exe`**。  
- 若你的 VS 把 exe 输出到别的路径，请到 **BlLauncher 项目属性 → 常规 → 输出目录** 查看实际位置，再在同一目录下复制一份为 **`shell_template.exe`**。

## 编译

### 方式一：直接打开解决方案（推荐）

用 **Visual Studio 2019/2022** 打开 `cpp\BsphpVmp.sln`，配置选 **x64**，菜单「生成 → 生成解决方案」。

输出目录示例：`cpp\x64\Release\` 下的 **`bl_launcher.exe`**、**`bl_keygen.exe`**、**`bl_packer.exe`**、**`shell_template.exe`**（以 VS 默认 OutDir 为准）。

工程已开启 **`/utf-8`**（源文件按 UTF-8 编译，避免中文界面字串在 GBK 代码页下报错），且各项目使用独立 **`IntDir`**，避免 MSB8028。若仍见旧警告，请先 **「清理解决方案」** 再重新生成。

## 内嵌公钥与 payload（打进 bl_launcher.exe）

1. 将 **`public.blb`**、**`payload.exe`** 放到 **`launcher\embedded\`**（可运行 **`prepare_embedded.bat`** 辅助复制，脚本内路径请按实际修改）。
2. 重新 **生成 BlLauncher**。资源编译器会把这两个文件作为 `RCDATA` 链入 exe。
3. **运行时优先级**  
   - **公钥**：资源块长度 ≥ 200 字节且 CNG 导入成功 → 用内嵌；否则使用同目录 **`public.blb`**。  
   - **主程序**：资源为有效 PE（`MZ` 头）→ 释放到 **`%TEMP%`** 下临时 exe 再启动；否则启动同目录 **`payload.exe`**。  
4. 内嵌的 payload 释放在临时目录后 **不会立即删除**（避免子进程未加载完就被删）；可定期清理 `%TEMP%\bsphp_payload_*.exe`。  

仓库中 **`embedded`** 里带有 **1 字节占位文件**，便于空仓库也能通过编译；发版前务必换成真实 `public.blb` 与你的程序。

> 工程默认使用 **VS2019** 平台工具集 **v142**。若使用 **VS2022**，可在各项目属性中将「平台工具集」改为 **v143**。

### 方式二：CMake

```bat
cd cpp
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
```

（若使用 VS2022，将生成器改为 `"Visual Studio 17 2022"`。）

输出：`build\Release\` 下的 `bl_launcher.exe`、`bl_keygen.exe`、`bl_packer.exe`（CMake 不会自动生成 `shell_template.exe`，请手动将 `bl_launcher.exe` 复制为同目录下的 `shell_template.exe`）。

## 发行流程简述（手动文件方式，可选）

若不用加壳工具，也可让客户目录里同时有：`bl_launcher.exe`、`public.blb`、`payload.exe`（你的程序改名）。更推荐上文 **bl_packer 一键生成单文件**。

1. 运行 **注册机** →「生成密钥对」。
2. **加壳工具** 生成单 exe；或手动把 `bl_launcher` + `public.blb` + `payload.exe` 同目录打包给客户。
3. 客户打开 → 机器码发你 → 注册机生成授权码 → 客户粘贴后启动。

## 编译报错 LNK1104（无法打开 bl_packer.exe）/ 清理时「访问被拒绝」

几乎总是 **exe 正在被占用** 或 **杀毒/Defender 锁住文件**。请依次试：

1. **关掉正在运行的 `bl_packer.exe`**（任务管理器里结束进程）。  
2. 关闭资源管理器里对该 exe 的预览、或 **暂时关掉实时杀毒** / 把工程目录加入 Windows Defender **排除项**。  
3. 在资源管理器中 **手动删除** `cpp\x64\Debug\bl_packer.exe`（若提示被占用，回到第 1 步）。  
4. VS 里 **「清理」再「重新生成」BlPacker**。  

工程已为 `BlPacker` 加上编译选项 **`/FS`**，减轻多项目并行生成时的锁冲突。

## 产品名

授权签名包含 **产品名** 字段；壳侧校验使用授权里保存的产品名。请保证注册机填写的 **产品名** 与签发时一致（默认示例 `myapp`）。

## 不绑机授权

注册机里「顾客机器码」填 **`*`** 可生成任意机器可用的码（慎用）。
