## 9.7\. Security Features
Device implementations MUST ensure compliance with security features in both the
kernel and platform as described below.

The Android Sandbox includes features that use the Security-Enhanced Linux
(SELinux) mandatory access control (MAC) system, seccomp sandboxing, and other
security features in the Linux kernel. Device implementations:

*   [C-0-1] MUST maintain compatibility with existing applications, even when
SELinux or any other security features are implemented below the Android
framework.
*   [C-0-2] MUST NOT have a visible user interface when a security
violation is detected and successfully blocked by the security feature
implemented below the Android framework, but MAY have a visible user interface
when an unblocked security violation occurs resulting in a successful exploit.
*   [C-0-3] MUST NOT make SELinux or any other security features implemented
below the Android framework configurable to the user or app developer.
*   [C-0-4]  MUST NOT allow an application that can affect another application
through an API (such as a Device Administration API) to configure a policy
that breaks compatibility.
*   [C-0-5] MUST split the media framework into multiple processes so that it
is possible to more narrowly grant access for each process as
[described](https://source.android.com/devices/media/framework-hardening.html#arch_changes)
in the Android Open Source Project site.
*   [C-0-6] MUST implement a kernel application sandboxing mechanism
which allows filtering of system calls using a configurable policy from
multithreaded programs. The upstream Android Open Source Project meets this
requirement through enabling the seccomp-BPF with threadgroup
synchronization (TSYNC) as described
[in the Kernel Configuration section of source.android.com](http://source.android.com/devices/tech/config/kernel.html#Seccomp-BPF-TSYNC).

Kernel integrity and self-protection features are integral to Android
security. Device implementations:

*   [C-0-7] MUST implement kernel stack buffer overflow protection mechanisms.
Examples of such mechanisms are `CC_STACKPROTECTOR_REGULAR` and
`CONFIG_CC_STACKPROTECTOR_STRONG`.
*   [C-0-8] MUST implement strict kernel memory protections where executable
code is read-only, read-only data is non-executable and non-writable, and
writable data is non-executable (e.g. `CONFIG_DEBUG_RODATA` or `CONFIG_STRICT_KERNEL_RWX`).
*   [C-0-9] MUST implement static and dynamic object size
bounds checking of copies between user-space and kernel-space
(e.g. `CONFIG_HARDENED_USERCOPY`) on devices originally shipping with API level
28 or higher.
*   [C-0-10] MUST NOT execute user-space memory when executing
in the kernel mode (e.g. hardware PXN, or emulated via
`CONFIG_CPU_SW_DOMAIN_PAN` or `CONFIG_ARM64_SW_TTBR0_PAN`) on devices
originally shipping with API level 28 or higher.
*   [C-0-11] MUST NOT read or write user-space memory in the
kernel outside of normal usercopy access APIs (e.g. hardware PAN, or
emulated via `CONFIG_CPU_SW_DOMAIN_PAN` or `CONFIG_ARM64_SW_TTBR0_PAN`)
on devices originally shipping with API level 28 or higher.
*   [C-0-12] MUST implement kernel page table isolation if the hardware is
vulnerable to CVE-2017-5754 on all devices originally shipping with API level
28 or higher (e.g. `CONFIG_PAGE_TABLE_ISOLATION` or
`CONFIG_UNMAP_KERNEL_AT_EL0`).
*   [C-0-13] MUST implement branch prediction hardening if the hardware is
vulnerable to CVE-2017-5715 on all devices originally shipping with API level
28 or higher (e.g. `CONFIG_HARDEN_BRANCH_PREDICTOR`).
*   [SR] STRONGLY RECOMMENDED to keep kernel data
which is written only during initialization marked read-only after
initialization (e.g. `__ro_after_init`).
*   [C-SR] Are STRONGLY RECOMMENDED to randomize the layout of the kernel code and
memory, and to avoid exposures that would compromise the randomization
(e.g. `CONFIG_RANDOMIZE_BASE` with bootloader entropy via the
[`/chosen/kaslr-seed Device Tree node`](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/chosen.txt)
or [`EFI_RNG_PROTOCOL`](https://docs.microsoft.com/en-us/windows-hardware/drivers/bringup/efi-rng-protocol)).

*   [C-SR] Are STRONGLY RECOMMENDED to enable control flow integrity (CFI) in
the kernel to provide additional protection against code-reuse attacks
(e.g. `CONFIG_CFI_CLANG` and `CONFIG_SHADOW_CALL_STACK`).
*   [C-SR] Are STRONGLY RECOMMENDED not to disable Control-Flow Integrity (CFI),
Shadow Call Stack (SCS) or Integer Overflow Sanitization (IntSan) on
components that have it enabled.
*   [C-SR] Are STRONGLY RECOMMENDED to enable CFI, SCS, and IntSan for any
additional security-sensitive userspace components as explained in
[CFI](https://source.android.com/devices/tech/debug/cfi) and
[IntSan](https://source.android.com/devices/tech/debug/intsan).
*   [C-SR] Are STRONGLY RECOMMENDED to enable stack initialization in the kernel
to prevent uses of uninitialized local variables (`CONFIG_INIT_STACK_ALL` or
`CONFIG_INIT_STACK_ALL_ZERO`).
Also, device implementations SHOULD NOT assume the value used by the compiler to
initialize the locals.
*   [C-SR] Are STRONGLY RECOMMENDED to enable heap initialization in the kernel
to prevent uses of uninitialized heap allocations
(`CONFIG_INIT_ON_ALLOC_DEFAULT_ON`) and they SHOULD NOT assume the value used by
the kernel to initialize those allocations.

If device implementations use a Linux kernel, they:

*   [C-1-1] MUST implement SELinux.
*   [C-1-2] MUST set SELinux to global enforcing mode.
*   [C-1-3] MUST configure all domains in enforcing mode. No permissive mode
domains are allowed, including domains specific to a device/vendor.
*   [C-1-4] MUST NOT modify, omit, or replace the neverallow rules present
within the system/sepolicy folder provided in the upstream Android Open Source
Project (AOSP) and the policy MUST compile with all neverallow rules present,
for both AOSP SELinux domains as well as device/vendor specific domains.
*   [C-1-5] MUST run third-party applications targeting API level 28 or higher
in per-application SELinux sandboxes with per-app SELinux restrictions on each
application's private data directory.
*   SHOULD retain the default SELinux policy provided in the system/sepolicy
folder of the upstream Android Open Source Project and only further add to this
policy for their own device-specific configuration.


If device implementations use kernel other than Linux, they:

*   [C-2-1] MUST use a mandatory access control system that is
equivalent to SELinux.

Android contains multiple defense-in-depth features that are integral to device
security.
