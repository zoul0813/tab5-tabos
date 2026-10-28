# Linux tooling validation — 2026-10-28

Host: Ubuntu 26.10. Existing Tab5 firmware reported ESP-IDF v5.4.4,
application version `4b53ae0`, ST7121 display. No firmware was changed or flashed.

## Automated checks

- Python tooling suite: 19 tests passed, including mount identity/ambiguity,
  explicit overrides, preflight rejection before writes, hash-failure preservation,
  serial failure/short-write handling without retries, and fresh-shell Bash activation.
- Host-environment suite: 3 tests passed, including absent Ubuntu SDL3 apt candidate.
- Existing application argument-preservation shell regression passed.
- Bash syntax and `git diff --check` passed.

Fresh-shell testing uses an isolated Bash-dependent activation fixture and fake make;
it does not require ESP-IDF or touch hardware. This checkout has no local ESP-IDF
installation and the host lacks SDL3. Complete app compilation and host CTest were
not run. Ubuntu 24.04 was not booted; no native macOS hardware test was performed.

## Physical checks

USB-C control: `/dev/ttyACM0`, Espressif VID/PID `303a:1001`.
USB-A MSC: `303a:4002`, FAT partition labeled `TAB5`, discovered at
`/run/media/zoul0813/TAB5` on USB disk `/dev/sdb`.

The uploader copied a documentation file to `share/tabos-linux-validation.txt`,
verified SHA-256, and successfully ran UDisks unmount and power-off. A subsequent
MSC request was accepted, confirming return to the sole-shell state.

An uninterrupted serial capture across a separate eject cycle recorded:

```text
tabos_usb_storage: USB storage session ended; restarting Tab5
rst:0xc (SW_CPU_RESET)
tabos_storage: Mounted microSD at /sdcard
```

The capture missed intermediate display/shell log lines while synchronous eject
was running; it therefore did not satisfy the diagnostic script's stronger
shell-log assertion. The software-reset and storage-remount evidence is direct.

During an earlier diagnostic, opening a second serial connection while MSC was
mounted triggered `CHIP_USB_UART_RESET`, despite DTR/RTS being configured false.
That diagnostic had removed the test file before the reset. A later copy triggered
Linux FAT `fat_free_clusters: deleting FAT entry beyond EOF` and a read-only
remount. The uploader stopped before eject and reported failure. These events
must not be represented as an entirely clean acceptance run.

The card was unmounted. Operator ran `sudo /usr/sbin/fsck.fat -n /dev/sdb1`;
it reported boot-sector dirty-byte mismatch at offset 65 and a set dirty bit,
with no file-chain repair proposed (240 files, 2516/3802815 clusters).
Operator then ran `sudo /usr/sbin/fsck.fat -a /dev/sdb1`; it cleared the dirty
state and reported 240 files, 2516/3802815 clusters. The card was remounted through
UDisks. The updated uploader completed another verified copy, unmount and power-off
with exit status 0. A new serial MSC request was accepted after that restart,
proving normal sole-shell recovery. The uploaded SHA-256 matched after remount.
The temporary test file was removed, writes synchronized, and final safe eject
completed successfully. No further operator action was required.

Production upload now refuses a new serial request whenever Linux already exposes
any `TAB5` USB filesystem, including an unmounted one or a mismatched mount override.
This prevents that serial-open reset hazard in the supported upload workflow.
