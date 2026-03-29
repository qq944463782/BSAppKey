//! v0 占位：真实加壳（改 PE、解密、OEP）在后续里程碑实现。
//! 授权逻辑请先用 `bl_license` + `bl_demo` 集成进你的程序。

use clap::Parser;

#[derive(Parser)]
#[command(name = "bl_pack")]
struct Cli {
    #[arg(long)]
    input: Option<String>,
}

fn main() {
    let _cli = Cli::parse();
    println!("bl_pack v0 — stub only.");
    println!("Rust 可独立完成壳核心；仅当需要 KB 级极简 stub 或与现成 C 运行时对接时，再考虑混合 C。");
}
