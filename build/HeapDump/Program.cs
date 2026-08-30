// HeapDump - NanaZip 开发调试工具
//
// 两个子命令：
//
//   HeapDump trace <target.exe> <args...> <dumpDir> [--exceptions <hex,hex>]
//     以调试器身份启动目标进程，监视异常。命中指定异常（默认堆损坏
//     0xC0000374 / 访问违例 0xC0000005）的 first-chance 时：
//       1. 打印崩溃线程的原始栈（模块+offset）
//       2. 写全量 minidump 到 dumpDir
//     之后放行异常，目标照常结束（WER 可继续接管）。
//
//   HeapDump sym <binary> <baseHex> <rva...>
//     用二进制旁的 PDB 解析 RVA 对应的符号名。
//
// 使用示例（见目录内 README.md）：
//   HeapDump trace NanaZip.Universal.Windows.exe "x test.zip -oC:\out" E:\dumps
//   HeapDump sym   NanaZip.Universal.Windows.exe 140000000 5CE86 CBE2
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

internal static class Program
{
    // ===== Win32: process / debug =====
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool CreateProcessW(string lpApplicationName, string lpCommandLine,
        IntPtr lpProcessAttributes, IntPtr lpThreadAttributes, bool bInheritHandles,
        uint dwCreationFlags, IntPtr lpEnvironment, string lpCurrentDirectory,
        ref STARTUPINFOW lpStartupInfo, out PROCESS_INFORMATION lpProcessInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool WaitForDebugEvent(out DEBUG_EVENT ev, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool ContinueDebugEvent(uint dwProcessId, uint dwThreadId, uint dwContinueStatus);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool TerminateProcess(IntPtr hProcess, uint exitCode);

    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(IntPtr h);

    [DllImport("dbghelp.dll", SetLastError = true)]
    private static extern bool MiniDumpWriteDump(IntPtr hProcess, uint processId, IntPtr hFile,
        int dumpType, IntPtr exceptionParam, IntPtr userStreamParam, IntPtr callbackParam);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateFileW(string name, uint desiredAccess, uint shareMode,
        IntPtr securityAttributes, uint creationDisposition, uint flagsAndAttributes, IntPtr templateFile);

    const uint DEBUG_ONLY_THIS_PROCESS = 0x00000002;
    const uint DBG_CONTINUE = 0x00010002;
    const uint DBG_EXCEPTION_NOT_HANDLED = 0x80010001;
    const int EXCEPTION_DEBUG_EVENT = 1;
    const int EXIT_PROCESS_DEBUG_EVENT = 5;
    const uint EXCEPTION_ACCESS_VIOLATION = 0xC0000005;
    const uint EXCEPTION_HEAP_CORRUPTION = 0xC0000374;
    const int MiniDumpWithFullMemory = 2;

    [StructLayout(LayoutKind.Sequential)]
    private struct STARTUPINFOW
    {
        public uint cb;
        public IntPtr lpReserved, lpDesktop, lpTitle;
        public uint dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
        public ushort wShowWindow, cbReserved2;
        public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PROCESS_INFORMATION
    {
        public IntPtr hProcess, hThread;
        public uint dwProcessId, dwThreadId;
    }

    // x64 DEBUG_EVENT: header 16 bytes, union starts at 16, total 184.
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    private struct DEBUG_EVENT
    {
        public uint dwDebugEventCode;
        public uint dwProcessId;
        public uint dwThreadId;
        public ulong Data;   // union bytes 16..23
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 19)]
        public ulong[] Rest; // union bytes 24..167
    }

    private sealed class ExceptionRecord
    {
        public uint Code;
        public ulong Address;
        public uint NumberParameters;
        public ulong Parameter0;
        public ulong Parameter1;
    }

    // union bytes 16..23 = ExceptionCode(4) + ExceptionFlags(4)
    // bytes 24..31 = ExceptionRecord pointer (unused)
    // bytes 32..39 = ExceptionAddress
    // bytes 40..47 = NumberParameters(4) + pad
    // bytes 48..55 = ExceptionInformation[0]
    // bytes 56..63 = ExceptionInformation[1]
    private static ExceptionRecord ParseException(DEBUG_EVENT ev)
    {
        var er = new ExceptionRecord
        {
            Code = (uint)(ev.Data & 0xFFFFFFFF),
            Address = ev.Rest is { Length: >= 2 } ? ev.Rest[1] : 0,
            NumberParameters = ev.Rest is { Length: >= 3 } ? (uint)(ev.Rest[2] & 0xFFFFFFFF) : 0,
            Parameter0 = ev.Rest is { Length: >= 4 } ? ev.Rest[3] : 0,
            Parameter1 = ev.Rest is { Length: >= 5 } ? ev.Rest[4] : 0,
        };
        return er;
    }

    private static bool TryDump(IntPtr hProcess, uint pid, string path)
    {
        IntPtr hFile = CreateFileW(path, 0x40000000u /*GENERIC_WRITE*/, 1u /*FILE_SHARE_READ*/,
            IntPtr.Zero, 2u /*CREATE_ALWAYS*/, 128u /*FILE_ATTRIBUTE_NORMAL*/, IntPtr.Zero);
        if (hFile == IntPtr.Zero || hFile == new IntPtr(-1))
        {
            Console.Error.WriteLine("  CreateFile for dump failed: " + Marshal.GetLastWin32Error());
            return false;
        }
        bool ok = MiniDumpWriteDump(hProcess, pid, hFile, MiniDumpWithFullMemory,
            IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        CloseHandle(hFile);
        Console.WriteLine(ok
            ? "  dump written: " + path
            : "  MiniDumpWriteDump failed: " + Marshal.GetLastWin32Error());
        return ok;
    }

    private static int RunTrace(string exe, string args, string dumpDir, HashSet<uint> breakOn, bool killAfterDump)
    {
        if (!File.Exists(exe))
        {
            Console.Error.WriteLine("target not found: " + exe);
            return 2;
        }
        Directory.CreateDirectory(dumpDir);

        var si = new STARTUPINFOW();
        si.cb = (uint)Marshal.SizeOf<STARTUPINFOW>();
        if (!CreateProcessW(null, "\"" + exe + "\" " + args, IntPtr.Zero, IntPtr.Zero,
            false, DEBUG_ONLY_THIS_PROCESS, IntPtr.Zero, null, ref si, out var pi))
        {
            Console.Error.WriteLine("CreateProcess failed: " + Marshal.GetLastWin32Error());
            return 2;
        }
        Console.WriteLine("debuggee pid: " + pi.dwProcessId);

        string dumpPath = Path.Combine(dumpDir,
            Path.GetFileNameWithoutExtension(exe) + "_heap_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".dmp");
        bool dumped = false;

        while (true)
        {
            if (!WaitForDebugEvent(out var ev, 0xFFFFFFFF))
                break;

            uint status = DBG_CONTINUE;

            if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
            {
                var er = ParseException(ev);
                Console.WriteLine("exception 0x" + er.Code.ToString("X8")
                    + " at 0x" + er.Address.ToString("X16")
                    + " thread " + ev.dwThreadId);

                if (breakOn.Contains(er.Code) && !dumped)
                {
                    Console.WriteLine("=== crash stack (raw rsp scan) ===");
                    foreach (var line in StackScanner.Walk(pi.hProcess, ev.dwThreadId))
                        Console.WriteLine("  " + line);
                    dumped = TryDump(pi.hProcess, ev.dwProcessId, dumpPath);
                    if (dumped && killAfterDump)
                    {
                        TerminateProcess(pi.hProcess, er.Code);
                        Console.WriteLine("target terminated after dump");
                        break;
                    }
                }
                status = DBG_EXCEPTION_NOT_HANDLED;
            }
            else if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
            {
                ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
                break;
            }

            ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status);
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        Console.WriteLine(dumped ? "dump: " + dumpPath : "no dump captured");
        return 0;
    }

    // ===== sym subcommand =====
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool SymInitialize(IntPtr hProcess, string searchPath, bool invadeProcess);

    [DllImport("dbghelp.dll")]
    private static extern bool SymCleanup(IntPtr hProcess);

    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern ulong SymLoadModuleEx(IntPtr hProcess, IntPtr hFile, string imageName,
        string moduleName, ulong baseOfDll, uint sizeOfDll, IntPtr data, uint flags);

    [DllImport("dbghelp.dll", CharSet = CharSet.Unicode)]
    private static extern bool SymFromAddr(IntPtr hProcess, ulong address, out ulong displacement, IntPtr symbol);

    private static int RunSym(string binary, ulong baseAddr, string[] rvas)
    {
        IntPtr hProcess = new IntPtr(-1); // pseudo handle
        if (!SymInitialize(hProcess, null, false))
        {
            Console.Error.WriteLine("SymInitialize failed");
            return 2;
        }
        if (SymLoadModuleEx(hProcess, IntPtr.Zero, binary, null, baseAddr, 0, IntPtr.Zero, 0) == 0)
        {
            Console.Error.WriteLine("SymLoadModuleEx failed: " + Marshal.GetLastWin32Error());
            return 2;
        }
        foreach (var rvaText in rvas)
        {
            ulong rva = Convert.ToUInt64(rvaText, 16);
            IntPtr buf = Marshal.AllocHGlobal(2048);
            try
            {
                for (int b = 0; b < 2048; b++) Marshal.WriteByte(buf, b, 0);
                // SYMBOL_INFOW (dbghelp.h): SizeOfStruct is a ULONG (4 bytes!),
                // not 8. x64 layout: SizeOfStruct@0(4), TypeIndex@4(4),
                // Reserved@8(16), Index@24(4), Size@28(4), ModBase@32(8),
                // Flags@40(4), Value@48(8), Address@56(8), Register@64(4),
                // Scope@68(4), Tag@72(4), NameLen@76(4), MaxNameLen@80(4),
                // Name@84 (wchar). sizeof == 88.
                Marshal.WriteInt32(buf, 0, 88);        // SizeOfStruct = 88 (ULONG!)
                Marshal.WriteInt32(buf, 80, 1024);     // MaxNameLen
                bool ok = SymFromAddr(hProcess, baseAddr + rva, out ulong disp, buf);
                string name = "<empty>";
                if (ok)
                {
                    var sb = new System.Text.StringBuilder();
                    for (int off = 84; off < 2040; off += 2)
                    {
                        char ch = (char)Marshal.ReadInt16(buf, off);
                        if (ch == 0) break;
                        sb.Append(ch);
                    }
                    name = sb + " + 0x" + disp.ToString("X");
                }
                else name = "<no symbol> (err " + Marshal.GetLastWin32Error() + ")";
                Console.WriteLine("+0x" + rva.ToString("X") + " = " + name);
            }
            finally { Marshal.FreeHGlobal(buf); }
        }
        SymCleanup(hProcess);
        return 0;
    }

    private static int Main(string[] args)
    {
        if (args.Length >= 1 && args[0] == "trace" && args.Length >= 4)
        {
            string exe = args[1];
            string dumpDir = args[^1];
            string joined = string.Join(' ', args[2..^1]);
            var breakOn = new HashSet<uint> { EXCEPTION_HEAP_CORRUPTION, EXCEPTION_ACCESS_VIOLATION };
            bool kill = false;
            // extract options: --exceptions <list>, --kill
            var argList = new List<string>(joined.Split(' '));
            for (int i = argList.Count - 1; i >= 0; i--)
            {
                if (argList[i] == "--kill") { kill = true; argList.RemoveAt(i); }
                else if (argList[i] == "--exceptions" && i + 1 < argList.Count)
                {
                    breakOn.Clear();
                    foreach (var code in argList[i + 1].Split(','))
                        breakOn.Add(Convert.ToUInt32(code, 16));
                    argList.RemoveRange(i, 2);
                }
            }
            return RunTrace(exe, string.Join(' ', argList), dumpDir, breakOn, kill);
        }
        if (args.Length >= 1 && args[0] == "sym" && args.Length >= 4)
            return RunSym(args[1], Convert.ToUInt64(args[2], 16), args[3..]);

        Console.Error.WriteLine("""
            HeapDump - NanaZip 调试工具
            用法:
              HeapDump trace <target.exe> <args...> <dumpDir> [--exceptions hex,hex] [--kill]
              HeapDump sym   <binary> <baseHex> <rva...>
            """);
        return 1;
    }
}

// ===== raw stack scanner =====
// Reads the crashing thread's stack memory and prints qwords that fall
// inside a module image, as module+offset. CONTEXT is handled as a raw
// byte buffer (1232 bytes, RSP at 0x98, RIP at 0xF8) to avoid manual
// struct marshalling mistakes.
internal static class StackScanner
{
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenThread(uint desiredAccess, bool inheritHandle, uint threadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetThreadContext(IntPtr hThread, byte[] context);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr baseAddress,
        byte[] buffer, IntPtr size, out IntPtr read);

    [DllImport("psapi.dll", SetLastError = true)]
    private static extern bool EnumProcessModules(IntPtr hProcess, IntPtr[] modules, uint cb, out uint needed);

    [DllImport("psapi.dll", CharSet = CharSet.Unicode)]
    private static extern uint GetModuleFileNameEx(IntPtr hProcess, IntPtr hModule,
        System.Text.StringBuilder filename, uint size);

    [DllImport("psapi.dll")]
    private static extern bool GetModuleInformation(IntPtr hProcess, IntPtr hModule,
        out MODULEINFO info, uint cb);

    private const uint THREAD_GET_CONTEXT = 0x0008;
    private const uint THREAD_QUERY_INFORMATION = 0x0040;
    private const int CONTEXT_SIZE = 1232; // x64 CONTEXT
    private const int CONTEXT_RSP_OFFSET = 0x98;
    private const int CONTEXT_RIP_OFFSET = 0xF8;

    [StructLayout(LayoutKind.Sequential)]
    private struct MODULEINFO { public IntPtr BaseOfDll; public uint SizeOfImage; public IntPtr EntryPoint; }

    public static List<string> Walk(IntPtr hProcess, uint crashThreadId)
    {
        var result = new List<string>();
        var modules = BuildModuleList(hProcess);

        IntPtr hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, false, crashThreadId);
        if (hThread == IntPtr.Zero)
        {
            result.Add("OpenThread failed " + Marshal.GetLastWin32Error());
            return result;
        }

        // CONTEXT must be 16-byte aligned; allocate with slack and align.
        byte[] raw = new byte[CONTEXT_SIZE + 16];
        var pinned = GCHandle.Alloc(raw, GCHandleType.Pinned);
        ulong addr = (ulong)pinned.AddrOfPinnedObject();
        ulong aligned = (addr + 15) & ~15UL;
        int offset = (int)(aligned - addr);
        BitConverter.GetBytes(0x10003Fu).CopyTo(raw, offset + 0x30); // CONTEXT_ALL
        if (!GetThreadContext(hThread, raw))
        {
            result.Add("GetThreadContext failed " + Marshal.GetLastWin32Error());
            return result;
        }
        ulong rsp = BitConverter.ToUInt64(raw, offset + CONTEXT_RSP_OFFSET);
        ulong rip = BitConverter.ToUInt64(raw, offset + CONTEXT_RIP_OFFSET);
        result.Add("RIP=0x" + rip.ToString("X12") + "  RSP=0x" + rsp.ToString("X12"));

        byte[] buf = new byte[16384];
        if (!ReadProcessMemory(hProcess, (IntPtr)rsp, buf, (IntPtr)buf.Length, out IntPtr read))
        {
            result.Add("ReadProcessMemory failed " + Marshal.GetLastWin32Error());
            return result;
        }
        int count = (int)read / 8;
        for (int i = 0; i < count; i++)
        {
            ulong v = BitConverter.ToUInt64(buf, i * 8);
            string m = Resolve(v, modules);
            if (m != null)
                result.Add("rsp+0x" + (i * 8).ToString("X") + "  " + m);
        }
        return result;
    }

    private static List<Tuple<ulong, uint, string>> BuildModuleList(IntPtr hProcess)
    {
        var list = new List<Tuple<ulong, uint, string>>();
        var mods = new IntPtr[1024];
        if (EnumProcessModules(hProcess, mods, (uint)(IntPtr.Size * mods.Length), out uint needed))
        {
            int count = (int)(needed / IntPtr.Size);
            for (int i = 0; i < count; i++)
            {
                var sb = new System.Text.StringBuilder(1024);
                GetModuleFileNameEx(hProcess, mods[i], sb, 1024);
                if (GetModuleInformation(hProcess, mods[i], out MODULEINFO mi, (uint)Marshal.SizeOf<MODULEINFO>()))
                    list.Add(Tuple.Create((ulong)mi.BaseOfDll, mi.SizeOfImage, sb.ToString()));
            }
        }
        list.Sort((a, b) => a.Item1.CompareTo(b.Item1));
        return list;
    }

    private static string Resolve(ulong pc, List<Tuple<ulong, uint, string>> modules)
    {
        for (int i = modules.Count - 1; i >= 0; i--)
        {
            var m = modules[i];
            if (pc >= m.Item1 && pc < m.Item1 + m.Item2)
                return Path.GetFileName(m.Item3) + "+0x" + (pc - m.Item1).ToString("X");
        }
        return null;
    }
}
