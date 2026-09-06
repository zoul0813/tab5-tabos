# TabOS: zero to confident user

## Review findings

Reviewed README, all 21 pages under `docs/`, application READMEs, and relevant build, packaging, shell, storage, and SDK code. Compared current working tree, including uncommitted changes. No files changed during review; no hardware validation performed.

Main problem: substantial reference material exists, but beginner journey lacks clear sequence, dependable downloads, and recovery checkpoints.

| Priority | Gap | Evidence and consequence |
|---|---|---|
| Critical | Downloads incomplete for first use | [CI packaging](../.github/workflows/build.yml) ships host binaries/libraries and firmware, but no shell/apps. Host storage also uses [compiled checkout path](../platform/host/posix/storage_backend.c). Download-and-run needs packaging changes. |
| High | Hardware instructions omit essential context | [First-run section](../README.md#first-run-on-tab5-hardware) omits card preparation, cable/port distinction, expected intermediate boot failure before shell installation, and recovery. USB-A storage versus USB-C programming explanation buried in filesystem manual. |
| High | Linux transfer workflow incomplete | [App installer](../apps/build.sh) requires macOS `diskutil` after copying. Custom mount path alone does not make workflow portable. |
| High | Copyable application paths wrong | [Hello README](../apps/hello_elf/README.md) and [loader guide](../docs/elf-loader.md) use `hello.bin`; [SDK rules](../sdk/make/application.mk) produce extensionless `hello`. |
| Medium | Shell reference contradicts code | [Shell docs](../docs/shell.md) promise status after every application and deny multiple-drive search. [Implementation](../apps/shell/src/main.c) supports configurable semicolon-separated PATH and prints only nonzero status. `ls` is external utility. |
| Medium | Capability and SDK descriptions stale | README describes scaffold/future shell; [display page](../docs/display.md) denies implemented graphics/touch functionality. Filesystem page says 1 MiB default heap; SDK defaults to 256 KiB. Loader metadata example says ABI 1; current application ABI is 3. |
| Medium | Navigation hides important material | [Docs index](../docs/README.md) omits SDK, graphics API, camera. Pages mix everyday use, API contracts, implementation details, and hardware validation. |
| Medium | No complete learning progression | Missing guided shell/file exercises, consolidated utility reference, update/backup procedure, troubleshooting entry point, and optional first-app tutorial. Wi-Fi config shown without complete file-creation/transfer walkthrough. |

## Agreed direction and chapter structure

Audience: computer user comfortable with terminal, unfamiliar with TabOS. Downloads first; physical Tab5 primary route, simulator alternative. Required journey ends at confident use; programming optional.

Use short, linked Markdown chapters under `docs/getting-started/`:

1. **Before starting:** explain firmware versus SD applications, required hardware, supported computers, current limitations, and download selection.
2. **First boot on Tab5:** prepare card, download matching firmware/apps, flash through USB-C, transfer through USB-A MSC or card reader, safely eject, reach shell.
3. **Simulator alternative:** download, extract, launch, identify persistent storage; join common lessons at shell.
4. **First shell session:** recognize prompt, run `help`, `pwd`, `ls`, `hello`; explain command location, arguments, successful return, and scrollback.
5. **Files and applications:** map workstation paths to `T:/`, transfer sample text, inspect/copy/rename files, launch Starfall and clock, return to shell.
6. **Wi-Fi and time:** create `wifi.conf` on workstation, transfer it, check connection, synchronize UTC time; explain simulated host Wi-Fi.
7. **Keep system working:** preserve data/configuration, update firmware and apps together, recover missing shell, inspect serial output, reboot/shutdown.
8. **Optional source builds:** checkout, prerequisites, setup, build/run/test; Windows development through WSL2.
9. **Optional first C application:** build existing hello, change message, rebuild/install/run; then introduce independent project using SDK.

Every chapter states prerequisites, where commands run, expected visible result, common failures, and next step. Write guide in normal beginner-friendly prose. Introduce terminology when first needed.

## Documentation organization

- Root README becomes concise project overview, support matrix, and entry links.
- Docs landing page separates **Getting started**, **Task guides**, **User manual**, **SDK reference**, and **Contributor internals**.
- Keep existing page URLs; reorganize their content and navigation without wholesale moves. Extract repeated user tasks into canonical guides and link back.
- Add consolidated command manual covering built-ins and installed utilities, including limited options and absent shell features.
- Keep driver registers, ELF transport, concurrency details, and hardware-validation procedures outside beginner chapters.
- Correct confirmed discrepancies throughout docs and app READMEs. Audit examples against current defaults; distinguish implemented functionality from physically validated behavior.
- Add lightweight checks for local links, missing navigation entries, and obsolete executable examples. Preserve deliberately historical/test-fixture references.

## Onboarding implementation

### Complete downloads

- Build default RV32 applications once per workflow commit; include matching applications in both host packages and Tab5 distribution. Keep DOOM opt-in.
- Preserve existing artifact names; add Windows-friendly ZIP of complete Tab5 distribution. Include firmware metadata, `bin/`, flash helper, installation notes, commit identity, and checksums.
- Continue using GitHub Actions development artifacts. Explain download access and snapshot status; no new release-publishing infrastructure required.

### Portable host packages

- Add host `--rootfs PATH` override, defaulting to existing configured directory. Keep change within host storage boundary; public application ABI unchanged.
- Package launchers select bundled writable `rootfs` using absolute package location, independent of launch directory.
- Add bounded `--smoke-shell` check that boots actual bundled shell headlessly and verifies prompt. Existing `--smoke` skips startup application and cannot validate complete package.
- Make source app installation honor configured host root so custom configuration and app destination agree.

### Flashing and file transfer

- Add standalone Python firmware-package flash helper using included ESP-IDF flash metadata and explicit serial-port selection; support Windows COM ports and Unix ports. Retain existing source-build commands.
- Separate application copying from eject behavior. Validate destination before writes; preserve unrelated files; report copy and eject results separately.
- Keep macOS automatic eject. Linux/Windows flow provides completed-copy result followed by explicit OS safe-eject instructions; absence of `diskutil` must not turn completed copy into unexplained failure.
- Do not assume card label `TAB5`. Support explicit mounted-volume path and document locating it.
- Document manual card-reader route and credential-file editing; no new TabOS editor or interactive Wi-Fi feature required.

### Windows

- Baseline: Windows 11 x64. Native Windows handles flashing, file transfer, eject, and recovery. Optional WSL2 Ubuntu development handles source builds and Linux simulator.
- Keep flashing outside WSL so beginner workflow needs no USB forwarding. WSL GUI route uses WSLg, subject to actual validation. [Microsoft guidance](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps).
- Firmware helper follows Espressif's COM-port and flash-metadata support. [Esptool guidance](https://docs.espressif.com/projects/esptool/en/latest/esp32p4/esptool/basic-commands.html).

## Verification and delivery

- Reproduce download-to-shell from clean environments: macOS ARM64, Ubuntu 26.04 x64, Windows 11 with Tab5; separately validate WSL2 builds and simulator.
- Test extracted host packages outside checkout, including paths containing spaces, no installed SDK, correct bundled apps, and persistent files across restart.
- Test flash command construction without hardware; verify real flashing and MSC/card-reader workflows on physical Tab5.
- Check missing/wrong card, absent shell, mismatched applications, ambiguous serial port, failed copy, failed eject, and incorrect Wi-Fi configuration.
- Verify update instructions preserve user files and credentials. Run every tutorial command; capture actual prompt/output instead of inventing transcripts.
- Apply project-required build/test checks to tooling and host changes. Keep physical/Windows validation explicitly pending until exercised.
- Deliver in sequence: factual corrections/navigation → onboarding tooling/packages → tutorial chapters → clean-environment walkthrough and final polish.

Acceptance: reader reaches shell from downloads without compiling, completes file/app/network lessons without consulting internals, and can update or recover installation. Optional coding route produces modified application using documented steps.
