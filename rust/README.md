# BSPHP-VMP — Rust 授权栈（v0.1）

## 是否需要 C 写壳核心？

**不必须。** PE 解析、加密、Windows API（`windows` / `winapi` crate）在 Rust 里都能做；生态里也有现成 PE 库可参考。只有在这些情况下常看到 **Rust + 少量 C**：

- 需要 **极小体积** 的入口 stub（几百字节级），手写汇编/C 更常见；
- 复用 **闭源 C 库** 或客户强制要求 C ABI；
- 团队已有 C 壳，只做 **FFI 调用** Rust 里的验签逻辑。

本仓库 **授权验签** 用 Rust 即可；`bl_pack` 预留给后续壳工具。

## 构建

需安装 [Rust](https://rustup.rs/)，在 `rust` 目录执行：

```bash
cd rust
cargo build --release
```

产物：`target/release/bl_issue.exe`、`bl_demo.exe`、`bl_pack.exe`（Windows）。

## 快速试跑

```bash
# 1) 生成密钥（勿随客户端分发 signing.key）
bl_issue gen-key --secret-out signing.key --public-out verifying.key

# 2) 查看本机指纹（绑机授权时用）
bl_demo fingerprint

# 3) 签发 license（不绑机）
bl_issue sign --secret signing.key --product myapp --expiry_unix 1893456000 --hwid "*" --out license.json

# 4) 校验（客户端只带 verifying.key）
bl_demo verify --license license.json --public-key verifying.key
```

## License 文件格式

JSON 字段：`product`、`expiry_unix`、`hwid`（`*` 或指纹）、`sig_hex`。  
签名明文：`bl_lic/v1|{product}|{expiry_unix}|{hwid}`（UTF-8）。

## 与 BSPHP 的关系

当前 v0 为 **离线 Ed25519**；联网激活可在 `bl_license` 上增加 HTTP 客户端，请求体按 BSPHP API 文档拼接。
