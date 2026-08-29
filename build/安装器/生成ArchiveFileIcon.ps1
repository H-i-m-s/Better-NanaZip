# 生成 NanaZipArchiveFile.ico（多尺寸 PNG 压缩 ICO）
# 来源：MSIX 包 Assets\ArchiveFile.targetsize-<n>.png（文件类型专用图标）
# 产物：build\安装器\NanaZipArchiveFile.ico（供 NanaZipSetup.iss 的 DefaultIcon 引用）
# 用法：powershell -File 生成ArchiveFileIcon.ps1 [-Version 6.5.1827.0]
param(
    [string]$Version = '6.5.1827.0',
    [string]$OutputFile = (Join-Path $PSScriptRoot 'NanaZipArchiveFile.ico')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$bundle = "E:\NanaZip\Output\发布\$Version\NanaZipPackage_${Version}_x64.msixbundle"
if (-not (Test-Path $bundle)) { throw "找不到 bundle: $bundle" }

$tmp = Join-Path $env:TEMP "_ico_build_$PID"
New-Item -ItemType Directory -Path $tmp -Force | Out-Null
try {
    # 解出 x64 单包
    $z = [System.IO.Compression.ZipFile]::OpenRead($bundle)
    $msix = $z.Entries | Where-Object { $_.FullName -match 'x64\.msix$' } | Select-Object -First 1
    if (-not $msix) { throw 'bundle 中未找到 x64 msix' }
    $msixPath = Join-Path $tmp 'pkg.msix'
    [System.IO.Compression.ZipFileExtensions]::ExtractToFile($msix, $msixPath, $true)
    $z.Dispose()
    [System.IO.Compression.ZipFile]::ExtractToDirectory($msixPath, (Join-Path $tmp 'pkg'))

    # 提取 ArchiveFile 各尺寸
    $sizes = @(16, 24, 32, 48, 64, 128, 256)
    $pngFiles = @()
    foreach ($s in $sizes) {
        $png = Join-Path $tmp "pkg\Assets\ArchiveFile.targetsize-$s.png"
        if (-not (Test-Path $png)) { throw "缺少 ArchiveFile.targetsize-$s.png" }
        $pngFiles += $png
    }

    # 合成 PNG 压缩 ICO（Vista+ 支持；256 尺寸必须 PNG 条目）
    $count = $pngFiles.Count
    $offset = 6 + 16 * $count
    $entries = New-Object System.Collections.Generic.List[byte[]]
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    $bw.Write([uint16]0)          # reserved
    $bw.Write([uint16]1)          # type: icon
    $bw.Write([uint16]$count)     # count

    foreach ($png in $pngFiles) {
        $data = [System.IO.File]::ReadAllBytes($png)
        $size = [System.Drawing.Image]::FromFile($png)
        $w = $size.Width
        $h = $size.Height
        $size.Dispose()
        $bw.Write([byte]($(if ($w -ge 256) { 0 } else { $w })))
        $bw.Write([byte]($(if ($h -ge 256) { 0 } else { $h })))
        $bw.Write([byte]0)        # color count
        $bw.Write([byte]0)        # reserved
        $bw.Write([uint16]1)      # planes
        $bw.Write([uint16]32)     # bpp
        $bw.Write([uint32]$data.Length)
        $bw.Write([uint32]$offset)
        $offset += $data.Length
        $entries.Add($data)
    }
    foreach ($d in $entries) { $bw.Write($d) }
    $bw.Flush()

    [System.IO.File]::WriteAllBytes($OutputFile, $ms.ToArray())
    $bw.Dispose(); $ms.Dispose()

    $len = (Get-Item $OutputFile).Length
    Write-Output "已生成: $OutputFile ($([Math]::Round($len/1KB,1)) KB, $count 尺寸: $($sizes -join ','))"
}
finally {
    if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
}
