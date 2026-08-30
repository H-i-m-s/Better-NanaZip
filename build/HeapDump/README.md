# HeapDump — 堆崩溃抓取与符号定位工具

一次命令完成"复现崩溃 → 抓全量转储 → 打印崩溃栈 → 用 PDB 解析符号"的完整链路。
不需要 WinDbg/cdb/Debugging Tools，只要 .NET 8 SDK（编译时）。

## 背景

`0xC0000374`（堆损坏）的特点是**损坏点与崩溃点分离**：某处代码越界写坏堆内存后，
进程往往在很久之后的某次普通堆操作时才被堆管理器处决。事后看崩溃位置的代码
通常是无辜的，真正作案的在几秒前。

这个工具把"案发现场"和"案发过程"一起钉住：进程停在异常的 first-chance 瞬间
（堆还没被 WER 撕掉），此刻抓全量 dump + 扫描崩溃线程栈，一次拿到全部证据。

## 构建

```powershell
dotnet build E:\NanaZip\build\HeapDump\HeapDump.csproj -c Release
# 产物: E:\NanaZip\Output\Binaries\Release\net8.0\HeapDump.exe
```

## 子命令

### trace — 复现崩溃并抓 dump

```powershell
HeapDump trace <target.exe> <args...> <dumpDir> [--exceptions hex,hex] [--kill]
```

- 以 `DEBUG_ONLY_THIS_PROCESS` 启动 target（自己是它的调试器）
- 默认在 `0xC0000374`（堆损坏）或 `0xC0000005`（访问违例）的 first-chance 时：
  1. 打印崩溃线程的原始栈扫描（RSP 起 16KB 内落在模块镜像里的所有地址，
     格式 `模块名+偏移`）
  2. 写全量 minidump（含堆）到 dumpDir
- `--exceptions 80000003,C0000374` 自定义触发异常；`--kill` 抓完 dump 后立即
  终止目标（跳过 WER 弹窗）
- 未命中异常则目标正常运行退出，不干预

### sym — RVA 转符号

```powershell
HeapDump sym <binary> <baseHex> <rva...>
```

- binary 旁需有同名 PDB（Release 构建默认生成于同目录）
- baseHex 是假基址即可（如 `140000000`），因为 trace 输出的偏移就是 RVA
- 例：`HeapDump sym app.exe 140000000 5CE86 CBE2`

## 实战流程（以 0xC0000374 静默提取崩溃为例）

```powershell
# 1. 确认崩溃签名（退出码 -1073740940 = 0xC0000374）
$p = Start-Process <7zG.exe> -ArgumentList 'x test.zip' -PassThru -Wait
$p.ExitCode

# 2. 抓崩溃栈 + dump
HeapDump trace <7zG.exe> "x test.zip" E:\dumps

# 3. RVA 转符号（用崩溃栈打印的偏移）
HeapDump sym <7zG.exe> 140000000 5CE86 CBE2 2A9B9
```

输出形如：

```
+0x5CE86 = CExtractCallbackImp::Release + 0x26
+0xCBE2 = CArchiveExtractCallback::~CArchiveExtractCallback + 0x1E2
+0x2A9B9 = Extract + 0x1689
```

自上而下即"崩在哪 ← 谁调的"，第一行是爆点，往下是作案路径。

## 实现要点（改代码前必读）

1. **P/Invoke 必须 `CharSet.Unicode`**：`CreateProcessW` 不写 CharSet 会按 ANSI
   marshal 路径，报 error 2（文件不存在）——极难排查。
2. **DEBUG_EVENT x64 布局**：头部 16 字节（code/pid/tid/pad），union 从 16 开始，
   总长 184。ExceptionCode 在 union 的低 32 位。
3. **SYMBOL_INFOW 的 SizeOfStruct 是 4 字节 ULONG 不是 8**（dbghelp.h），
   `WriteInt64(buf,0,88)` 会把 TypeIndex 覆盖掉导致名字乱码/为空。
   Name 从偏移 92 起（wchar）。
4. **CONTEXT 必须 16 字节对齐**，x64 总长 1232 字节，RSP@0x98、RIP@0xF8。
   栈扫描法（ReadProcessMemory RSP 起 16KB）比 StackWalk64 更抗造，
   堆损坏场景下 dbghelp 的 unwind 数据常不可靠。

## 已知局限

- 栈扫描会混入少量"长得像返回地址的巧合数据"，但按 rsp 偏移排序后，
  真实调用链一眼可辨（连续、间隔合理）
- 全量 dump 等于进程内存大小（数百 MB），dumpDir 要留足空间
- minidump 分析仍需 WinDbg；本工具的栈打印通常已够定位，无需打开 dump

