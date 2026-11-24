# 驱动来源

`drivers\` 下的文件都是各厂商原版包解出来的，未做任何修改。这里记录来源，方便日后更新或核对。

| 目录 | 驱动包 | 版本 | 来源 |
|---|---|---|---|
| `ch34x` | CH341SER（WCH 官网） | 3.9.2024.09 | 本机 DriverStore 提取：`C:\Windows\System32\DriverStore\FileRepository\ch341ser.inf_amd64_34a3206305b40d57`（源自 wch.cn 官方安装包），与本仓库同机安装验证过 |
| `ch9102` | CH343SER.ZIP | 2.2.2026.05 | WCH 官方下载 `https://file.wch.cn/downloads/CH343SER_ZIP.html`（id=314），9 个文件平铺 |
| `cp210x` | CP210x Universal Windows Driver | 11.2.0.167 | 本机 DriverStore 提取：`silabser.inf_amd64_cf7f3958a3d83caf`（Silicon Labs 官方包），保留 INF 要求的 `x64\` 子目录 |
| `ftdi` | CDM 驱动包（ftdibus.inf + ftdiport.inf） | 2.12.36.20 | 微软更新目录 updateID `0d361860-6a8b-42f9-a22f-6499571bae54`；保留 `amd64\` 与 `i386\` 子目录（`i386\ftd2xx.dll` 被 amd64 段引用） |
| `pl2303` | ser2pl.inf + plser.inf | 3.8.38.2 / 5.1.11.0 | `ser2pl.inf` 来自 theAmberLion/Prolific 仓库（codeload 下载）；`plser.inf` 来自微软更新目录 updateID `4980e6aa-b668-4bbf-82c4-63950db054bc`。两个 INF 覆盖不同 HardwareID，缺一不可 |

## 校验

- 每个目录里的 `.cat` 都是 `Microsoft Windows Hardware Compatibility Publisher` 签名
  （`Get-AuthenticodeSignature` 结果为 `Valid`）。
- 已按各 INF 的 `[SourceDisksFiles.*]` 核对所需文件是否齐全：
  ch34x 的 amd64 段只需要 `CH341S64.SYS` + `CH341PTA64.DLL` + `CH341PORTSA64.DLL` + `CH341PT.DLL`
  （`CH341M64.SYS` 只用于 ARM64，`CH341PORTS.DLL` 只用于 32 位，故意没放）。
- 硬件 ID 汇总见 `src/drivers.cpp` 的 `kVipTable`（区间匹配）。

## 未收录

`drivers/_raw\` 里保留了上游原始压缩包（约 25 MB，已被 `.gitignore` 排除），
需要重新解包时用。FTDI 的 `ftdiport.inf` 与 `ftdibus.inf` 必须成对使用，
单独装其中一个会导致设备管理器出现"未知设备"。
