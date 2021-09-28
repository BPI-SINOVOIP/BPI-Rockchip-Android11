#define HELP_toybox_force_nommu "When using musl-libc on a nommu system, you'll need to say \"y\" here\nunless you used the patch in the mcm-buildall.sh script. You can also\nsay \"y\" here to test the nommu codepaths on an mmu system.\n\nA nommu system can't use fork(), it can only vfork() which suspends\nthe parent until the child calls exec() or exits. When a program\nneeds a second instance of itself to run specific code at the same\ntime as the parent, it must use a more complicated approach (such as\nexec(\"/proc/self/exe\") then pass data to the new child through a pipe)\nwhich is larger and slower, especially for things like toysh subshells\nthat need to duplicate a lot of internal state in the child process\nfork() gives you for free.\n\nLibraries like uclibc omit fork() on nommu systems, allowing\ncompile-time probes to select which codepath to use. But musl\nintentionally includes a broken version of fork() that always returns\n-ENOSYS on nommu systems, and goes out of its way to prevent any\ncross-compile compatible compile-time probes for a nommu system.\n(It doesn't even #define __MUSL__ in features.h.) Musl does this\ndespite the fact that a nommu system can't even run standard ELF\nbinaries (requiring specially packaged executables) because it wants\nto force every program to either include all nommu code in every\ninstance ever built, or drop nommu support altogether.\n\nBuilding a toolchain scripts/mcm-buildall.sh patches musl to fix this."

#define HELP_toybox_uid_usr "When commands like useradd/groupadd allocate user IDs, start here."

#define HELP_toybox_uid_sys "When commands like useradd/groupadd allocate system IDs, start here."

#define HELP_toybox_pedantic_args "Check arguments for commands that have no arguments."

#define HELP_toybox_debug "Enable extra checks for debugging purposes. All of them catch\nthings that can only go wrong at development time, not runtime."

#define HELP_toybox_norecurse "When one toybox command calls another, usually it just calls the new\ncommand's main() function rather than searching the $PATH and calling\nexec on another file (which is much slower).\n\nThis disables that optimization, so toybox will run external commands\n       even when it has a built-in version of that command. This requires\n       toybox symlinks to be installed in the $PATH, or re-invoking the\n       \"toybox\" multiplexer command by name."

#define HELP_toybox_free "When a program exits, the operating system will clean up after it\n(free memory, close files, etc). To save size, toybox usually relies\non this behavior. If you're running toybox under a debugger or\nwithout a real OS (ala newlib+libgloss), enable this to make toybox\nclean up after itself."

#define HELP_toybox_i18n "Support for UTF-8 character sets, and some locale support."

#define HELP_toybox_help_dashdash "Support --help argument in all commands, even ones with a NULL\noptstring. (Use TOYFLAG_NOHELP to disable.) Produces the same output\nas \"help command\". --version shows toybox version."

#define HELP_toybox_help "Include help text for each command."

#define HELP_toybox_float "Include floating point support infrastructure and commands that\nrequire it."

#define HELP_toybox_libz "Use libz for gz support."

#define HELP_toybox_libcrypto "Use faster hash functions out of external -lcrypto library."

#define HELP_toybox_smack "Include SMACK options in commands like ls for systems like Tizen."

#define HELP_toybox_selinux "Include SELinux options in commands such as ls, and add\nSELinux-specific commands such as chcon to the Android menu."

#define HELP_toybox_lsm_none "Don't try to achieve \"watertight\" by plugging the holes in a\ncollander, instead use conventional unix security (and possibly\nLinux Containers) for a simple straightforward system."

#define HELP_toybox_suid "Support for the Set User ID bit, to install toybox suid root and drop\npermissions for commands which do not require root access. To use\nthis change ownership of the file to the root user and set the suid\nbit in the file permissions:\n\nchown root:root toybox; chmod +s toybox\n\nprompt \"Security Blanket\"\ndefault TOYBOX_LSM_NONE\nhelp\nSelect a Linux Security Module to complicate your system\nuntil you can't find holes in it."

#define HELP_toybox "usage: toybox [--long | --help | --version | [command] [arguments...]]\n\nWith no arguments, shows available commands. First argument is\nname of a command to run, followed by any arguments to that command.\n\n--long	Show path to each command\n\nTo install command symlinks with paths, try:\n  for i in $(/bin/toybox --long); do ln -s /bin/toybox $i; done\nor all in one directory:\n  for i in $(./toybox); do ln -s toybox $i; done; PATH=$PWD:$PATH\n\nMost toybox commands also understand the following arguments:\n\n--help		Show command help (only)\n--version	Show toybox version (only)\n\nThe filename \"-\" means stdin/stdout, and \"--\" stops argument parsing.\n\nNumerical arguments accept a single letter suffix for\nkilo, mega, giga, tera, peta, and exabytes, plus an additional\n\"d\" to indicate decimal 1000's instead of 1024.\n\nDurations can be decimal fractions and accept minute (\"m\"), hour (\"h\"),\nor day (\"d\") suffixes (so 0.1m = 6s)."

#define HELP_setenforce "usage: setenforce [enforcing|permissive|1|0]\n\nSets whether SELinux is enforcing (1) or permissive (0)."

#define HELP_sendevent "usage: sendevent DEVICE TYPE CODE VALUE\n\nSends a Linux input event."

#define HELP_runcon "usage: runcon CONTEXT COMMAND [ARGS...]\n\nRun a command in a specified security context."

#define HELP_restorecon "usage: restorecon [-D] [-F] [-R] [-n] [-v] FILE...\n\nRestores the default security contexts for the given files.\n\n-D	Apply to /data/data too\n-F	Force reset\n-R	Recurse into directories\n-n	Don't make any changes; useful with -v to see what would change\n-v	Verbose"

#define HELP_log "usage: log [-p PRI] [-t TAG] MESSAGE...\n\nLogs message to logcat.\n\n-p	Use the given priority instead of INFO:\n	d: DEBUG  e: ERROR  f: FATAL  i: INFO  v: VERBOSE  w: WARN  s: SILENT\n-t	Use the given tag instead of \"log\""

#define HELP_load_policy "usage: load_policy FILE\n\nLoad the specified SELinux policy file."

#define HELP_getenforce "usage: getenforce\n\nShows whether SELinux is disabled, enforcing, or permissive."

#define HELP_skeleton_alias "usage: skeleton_alias [-dq] [-b NUMBER]\n\nExample of a second command with different arguments in the same source\nfile as the first. This allows shared infrastructure not added to lib/."

#define HELP_skeleton "usage: skeleton [-a] [-b STRING] [-c NUMBER] [-d LIST] [-e COUNT] [...]\n\nTemplate for new commands. You don't need this.\n\nWhen creating a new command, copy this file and delete the parts you\ndon't need. Be sure to replace all instances of \"skeleton\" (upper and lower\ncase) with your new command name.\n\nFor simple commands, \"hello.c\" is probably a better starting point."

#define HELP_logwrapper "usage: logwrapper ...\n\nAppend command line to $WRAPLOG, then call second instance\nof command in $PATH."

#define HELP_hostid "usage: hostid\n\nPrint the numeric identifier for the current host."

#define HELP_hello "usage: hello\n\nA hello world program.\n\nMostly used as a simple template for adding new commands.\nOccasionally nice to smoketest kernel booting via \"init=/usr/bin/hello\"."

#define HELP_demo_utf8towc "usage: demo_utf8towc\n\nPrint differences between toybox's utf8 conversion routines vs libc du jour."

#define HELP_demo_scankey "usage: demo_scankey\n\nMove a letter around the screen. Hit ESC to exit."

#define HELP_demo_number "usage: demo_number [-hsbi] NUMBER...\n\n-b	Use \"B\" for single byte units (HR_B)\n-d	Decimal units\n-h	Human readable\n-s	Space between number and units (HR_SPACE)"

#define HELP_demo_many_options "usage: demo_many_options -[a-zA-Z]\n\nPrint the optflags value of the command arguments, in hex."

#define HELP_umount "usage: umount [-a [-t TYPE[,TYPE...]]] [-vrfD] [DIR...]\n\nUnmount the listed filesystems.\n\n-a	Unmount all mounts in /proc/mounts instead of command line list\n-D	Don't free loopback device(s)\n-f	Force unmount\n-l	Lazy unmount (detach from filesystem now, close when last user does)\n-n	Don't use /proc/mounts\n-r	Remount read only if unmounting fails\n-t	Restrict \"all\" to mounts of TYPE (or use \"noTYPE\" to skip)\n-v	Verbose"

#define HELP_sync "usage: sync\n\nWrite pending cached data to disk (synchronize), blocking until done."

#define HELP_su "usage: su [-lp] [-u UID] [-g GID,...] [-s SHELL] [-c CMD] [USER [COMMAND...]]\n\nSwitch user, prompting for password of new user when not run as root.\n\nWith one argument, switch to USER and run user's shell from /etc/passwd.\nWith no arguments, USER is root. If COMMAND line provided after USER,\nexec() it as new USER (bypasing shell). If -u or -g specified, first\nargument (if any) isn't USER (it's COMMAND).\n\nfirst argument is USER name to switch to (which must exist).\nNon-root users are prompted for new user's password.\n\n-s	Shell to use (default is user's shell from /etc/passwd)\n-c	Command line to pass to -s shell (ala sh -c \"CMD\")\n-l	Reset environment as if new login.\n-u	Switch to UID instead of USER\n-g	Switch to GID (only root allowed, can be comma separated list)\n-p	Preserve environment (except for $PATH and $IFS)"

#define HELP_seq "usage: seq [-w|-f fmt_str] [-s sep_str] [first] [increment] last\n\nCount from first to last, by increment. Omitted arguments default\nto 1. Two arguments are used as first and last. Arguments can be\nnegative or floating point.\n\n-f	Use fmt_str as a printf-style floating point format string\n-s	Use sep_str as separator, default is a newline character\n-w	Pad to equal width with leading zeroes"

#define HELP_pidof "usage: pidof [-s] [-o omitpid[,omitpid...]] [NAME]...\n\nPrint the PIDs of all processes with the given names.\n\n-s	Single shot, only return one pid\n-o	Omit PID(s)\n-x	Match shell scripts too"

#define HELP_passwd_sad "Password changes are checked to make sure they're at least 6 chars long,\ndon't include the entire username (but not a subset of it), or the entire\nprevious password (but changing password1, password2, password3 is fine).\nThis heuristic accepts \"aaaaaa\" and \"123456\"."

#define HELP_passwd "usage: passwd [-a ALGO] [-dlu] [USER]\n\nUpdate user's authentication tokens. Defaults to current user.\n\n-a ALGO	Encryption method (des, md5, sha256, sha512) default: des\n-d		Set password to ''\n-l		Lock (disable) account\n-u		Unlock (enable) account"

#define HELP_mount "usage: mount [-afFrsvw] [-t TYPE] [-o OPTION,] [[DEVICE] DIR]\n\nMount new filesystem(s) on directories. With no arguments, display existing\nmounts.\n\n-a	Mount all entries in /etc/fstab (with -t, only entries of that TYPE)\n-O	Only mount -a entries that have this option\n-f	Fake it (don't actually mount)\n-r	Read only (same as -o ro)\n-w	Read/write (default, same as -o rw)\n-t	Specify filesystem type\n-v	Verbose\n\nOPTIONS is a comma separated list of options, which can also be supplied\nas --longopts.\n\nAutodetects loopback mounts (a file on a directory) and bind mounts (file\non file, directory on directory), so you don't need to say --bind or --loop.\nYou can also \"mount -a /path\" to mount everything in /etc/fstab under /path,\neven if it's noauto. DEVICE starting with UUID= is identified by blkid -U."

#define HELP_mktemp "usage: mktemp [-dqu] [-p DIR] [TEMPLATE]\n\nSafely create a new file \"DIR/TEMPLATE\" and print its name.\n\n-d	Create directory instead of file (--directory)\n-p	Put new file in DIR (--tmpdir)\n-q	Quiet, no error messages\n-t	Prefer $TMPDIR > DIR > /tmp (default DIR > $TMPDIR > /tmp)\n-u	Don't create anything, just print what would be created\n\nEach X in TEMPLATE is replaced with a random printable character. The\ndefault TEMPLATE is tmp.XXXXXXXXXX."

#define HELP_mknod_z "usage: mknod [-Z CONTEXT] ...\n\n-Z	Set security context to created file"

#define HELP_mknod "usage: mknod [-m MODE] NAME TYPE [MAJOR MINOR]\n\nCreate a special file NAME with a given type. TYPE is b for block device,\nc or u for character device, p for named pipe (which ignores MAJOR/MINOR).\n\n-m	Mode (file permissions) of new device, in octal or u+x format"

#define HELP_sha512sum "See sha1sum"

#define HELP_sha384sum "See sha1sum"

#define HELP_sha256sum "See sha1sum"

#define HELP_sha224sum "See sha1sum"

#define HELP_sha1sum "usage: sha?sum [-bcs] [FILE]...\n\nCalculate sha hash for each input file, reading from stdin if none. Output\none hash (40 hex digits for sha1, 56 for sha224, 64 for sha256, 96 for sha384,\nand 128 for sha512) for each input file, followed by filename.\n\n-b	Brief (hash only, no filename)\n-c	Check each line of each FILE is the same hash+filename we'd output\n-s	No output, exit status 0 if all hashes match, 1 otherwise"

#define HELP_md5sum "usage: md5sum [-bcs] [FILE]...\n\nCalculate md5 hash for each input file, reading from stdin if none.\nOutput one hash (32 hex digits) for each input file, followed by filename.\n\n-b	Brief (hash only, no filename)\n-c	Check each line of each FILE is the same hash+filename we'd output\n-s	No output, exit status 0 if all hashes match, 1 otherwise"

#define HELP_killall "usage: killall [-l] [-iqv] [-SIGNAL|-s SIGNAL] PROCESS_NAME...\n\nSend a signal (default: TERM) to all processes with the given names.\n\n-i	Ask for confirmation before killing\n-l	Print list of all available signals\n-q	Don't print any warnings or error messages\n-s	Send SIGNAL instead of SIGTERM\n-v	Report if the signal was successfully sent\n-w	Wait until all signaled processes are dead"

#define HELP_dnsdomainname "usage: dnsdomainname\n\nShow domain this system belongs to (same as hostname -d)."

#define HELP_hostname "usage: hostname [-bdsf] [-F FILENAME] [newname]\n\nGet/set the current hostname.\n\n-b	Set hostname to 'localhost' if otherwise unset\n-d	Show DNS domain name (no host)\n-f	Show fully-qualified name (host+domain, FQDN)\n-F	Set hostname to contents of FILENAME\n-s	Show short host name (no domain)"

#define HELP_zcat "usage: zcat [FILE...]\n\nDecompress files to stdout. Like `gzip -dc`.\n\n-f	Force: allow read from tty"

#define HELP_gunzip "usage: gunzip [-cfk] [FILE...]\n\nDecompress files. With no files, decompresses stdin to stdout.\nOn success, the input files are removed and replaced by new\nfiles without the .gz suffix.\n\n-c	Output to stdout (act as zcat)\n-f	Force: allow read from tty\n-k	Keep input files (default is to remove)"

#define HELP_gzip "usage: gzip [-19cdfk] [FILE...]\n\nCompress files. With no files, compresses stdin to stdout.\nOn success, the input files are removed and replaced by new\nfiles with the .gz suffix.\n\n-c	Output to stdout\n-d	Decompress (act as gunzip)\n-f	Force: allow overwrite of output file\n-k	Keep input files (default is to remove)\n-#	Compression level 1-9 (1:fastest, 6:default, 9:best)"

#define HELP_dmesg "usage: dmesg [-Cc] [-r|-t|-T] [-n LEVEL] [-s SIZE] [-w]\n\nPrint or control the kernel ring buffer.\n\n-C	Clear ring buffer without printing\n-c	Clear ring buffer after printing\n-n	Set kernel logging LEVEL (1-9)\n-r	Raw output (with <level markers>)\n-S	Use syslog(2) rather than /dev/kmsg\n-s	Show the last SIZE many bytes\n-T	Human readable timestamps\n-t	Don't print timestamps\n-w	Keep waiting for more output (aka --follow)"

#define HELP_tunctl "usage: tunctl [-dtT] [-u USER] NAME\n\nCreate and delete tun/tap virtual ethernet devices.\n\n-T	Use tap (ethernet frames) instead of tun (ip packets)\n-d	Delete tun/tap device\n-t	Create tun/tap device\n-u	Set owner (user who can read/write device without root access)"

#define HELP_sntp "usage: sntp [-saSdDq] [-r SHIFT] [-mM[ADDRESS]] [-p PORT] [SERVER]\n\nSimple Network Time Protocol client. Query SERVER and display time.\n\n-p	Use PORT (default 123)\n-s	Set system clock suddenly\n-a	Adjust system clock gradually\n-S	Serve time instead of querying (bind to SERVER address if specified)\n-m	Wait for updates from multicast ADDRESS (RFC 4330 default 224.0.1.1)\n-M	Multicast server on ADDRESS (deault 224.0.0.1)\n-t	TTL (multicast only, default 1)\n-d	Daemonize (run in background re-querying )\n-D	Daemonize but stay in foreground: re-query time every 1000 seconds\n-r	Retry shift (every 1<<SHIFT seconds)\n-q	Quiet (don't display time)"

#define HELP_rfkill "usage: rfkill COMMAND [DEVICE]\n\nEnable/disable wireless devices.\n\nCommands:\nlist [DEVICE]   List current state\nblock DEVICE    Disable device\nunblock DEVICE  Enable device\n\nDEVICE is an index number, or one of:\nall, wlan(wifi), bluetooth, uwb(ultrawideband), wimax, wwan, gps, fm."

#define HELP_ping "usage: ping [OPTIONS] HOST\n\nCheck network connectivity by sending packets to a host and reporting\nits response.\n\nSend ICMP ECHO_REQUEST packets to ipv4 or ipv6 addresses and prints each\necho it receives back, with round trip time. Returns true if host alive.\n\nOptions:\n-4, -6		Force IPv4 or IPv6\n-c CNT		Send CNT many packets (default 3, 0 = infinite)\n-f		Flood (print . and \\b to show drops, default -c 15 -i 0.2)\n-i TIME		Interval between packets (default 1, need root for < .2)\n-I IFACE/IP	Source interface or address\n-m MARK		Tag outgoing packets using SO_MARK\n-q		Quiet (stops after one returns true if host is alive)\n-s SIZE		Data SIZE in bytes (default 56)\n-t TTL		Set Time To Live (number of hops)\n-W SEC		Seconds to wait for response after last -c packet (default 3)\n-w SEC		Exit after this many seconds"

#define HELP_netstat "usage: netstat [-pWrxwutneal]\n\nDisplay networking information. Default is netstat -tuwx\n\n-r	Routing table\n-a	All sockets (not just connected)\n-l	Listening server sockets\n-t	TCP sockets\n-u	UDP sockets\n-w	Raw sockets\n-x	Unix sockets\n-e	Extended info\n-n	Don't resolve names\n-W	Wide display\n-p	Show PID/program name of sockets"

#define HELP_netcat "usage: netcat [-46Ut] [-lL COMMAND...] [-u] [-wpq #] [-s addr] {IPADDR PORTNUM|-f FILENAME}\n\nForward stdin/stdout to a file or network connection.\n\n-4	Force IPv4\n-6	Force IPv6\n-L	Listen for multiple incoming connections (server mode)\n-U	Use a UNIX domain socket\n-W	SECONDS timeout for more data on an idle connection\n-f	Use FILENAME (ala /dev/ttyS0) instead of network\n-l	Listen for one incoming connection\n-p	Local port number\n-q	Quit SECONDS after EOF on stdin, even if stdout hasn't closed yet\n-s	Local source address\n-t	Allocate tty (must come before -l or -L)\n-u	Use UDP\n-w	SECONDS timeout to establish connection\n\nUse \"stty 115200 -F /dev/ttyS0 && stty raw -echo -ctlecho\" with\nnetcat -f to connect to a serial port.\n\nThe command line after -l or -L is executed (as a child process) to handle\neach incoming connection. If blank -l waits for a connection and forwards\nit to stdin/stdout. If no -p specified, -l prints port it bound to and\nbackgrounds itself (returning immediately).\n\nFor a quick-and-dirty server, try something like:\nnetcat -s 127.0.0.1 -p 1234 -tL /bin/bash -l"

#define HELP_microcom "usage: microcom [-s SPEED] [-X] DEVICE\n\nSimple serial console.\n\n-s	Set baud rate to SPEED\n-X	Ignore ^@ (send break) and ^] (exit)"

#define HELP_ifconfig "usage: ifconfig [-aS] [INTERFACE [ACTION...]]\n\nDisplay or configure network interface.\n\nWith no arguments, display active interfaces. First argument is interface\nto operate on, one argument by itself displays that interface.\n\n-a	All interfaces displayed, not just active ones\n-S	Short view, one line per interface\n\nStandard ACTIONs to perform on an INTERFACE:\n\nADDR[/MASK]        - set IPv4 address (1.2.3.4/5) and activate interface\nadd|del ADDR[/LEN] - add/remove IPv6 address (1111::8888/128)\nup|down            - activate or deactivate interface\n\nAdvanced ACTIONs (default values usually suffice):\n\ndefault          - remove IPv4 address\nnetmask ADDR     - set IPv4 netmask via 255.255.255.0 instead of /24\ntxqueuelen LEN   - number of buffered packets before output blocks\nmtu LEN          - size of outgoing packets (Maximum Transmission Unit)\nbroadcast ADDR   - Set broadcast address\npointopoint ADDR - PPP and PPPOE use this instead of \"route add default gw\"\nhw TYPE ADDR     - set hardware (mac) address (type = ether|infiniband)\n\nFlags you can set on an interface (or -remove by prefixing with -):\n\narp       - don't use Address Resolution Protocol to map LAN routes\npromisc   - don't discard packets that aren't to this LAN hardware address\nmulticast - force interface into multicast mode if the driver doesn't\nallmulti  - promisc for multicast packets"

#define HELP_ftpput "An ftpget that defaults to -s instead of -g"

#define HELP_ftpget "usage: ftpget [-cvgslLmMdD] [-P PORT] [-p PASSWORD] [-u USER] HOST [LOCAL] REMOTE\n\nTalk to ftp server. By default get REMOTE file via passive anonymous\ntransfer, optionally saving under a LOCAL name. Can also send, list, etc.\n\n-c	Continue partial transfer\n-p	Use PORT instead of \"21\"\n-P	Use PASSWORD instead of \"ftpget@\"\n-u	Use USER instead of \"anonymous\"\n-v	Verbose\n\nWays to interact with FTP server:\n-d	Delete file\n-D	Remove directory\n-g	Get file (default)\n-l	List directory\n-L	List (filenames only)\n-m	Move file on server from LOCAL to REMOTE\n-M	mkdir\n-s	Send file"

#define HELP_yes "usage: yes [args...]\n\nRepeatedly output line until killed. If no args, output 'y'."

#define HELP_xxd "usage: xxd [-c n] [-g n] [-i] [-l n] [-o n] [-p] [-r] [-s n] [file]\n\nHexdump a file to stdout.  If no file is listed, copy from stdin.\nFilename \"-\" is a synonym for stdin.\n\n-c n	Show n bytes per line (default 16)\n-g n	Group bytes by adding a ' ' every n bytes (default 2)\n-i	Include file output format (comma-separated hex byte literals)\n-l n	Limit of n bytes before stopping (default is no limit)\n-o n	Add n to display offset\n-p	Plain hexdump (30 bytes/line, no grouping)\n-r	Reverse operation: turn a hexdump into a binary file\n-s n	Skip to offset n"

#define HELP_which "usage: which [-a] filename ...\n\nSearch $PATH for executable files matching filename(s).\n\n-a	Show all matches"

#define HELP_watch "usage: watch [-teb] [-n SEC] PROG ARGS\n\nRun PROG every -n seconds, showing output. Hit q to quit.\n\n-n	Loop period in seconds (default 2)\n-t	Don't print header\n-e	Exit on error\n-b	Beep on command error\n-x	Exec command directly (vs \"sh -c\")"

#define HELP_w "usage: w\n\nShow who is logged on and since how long they logged in."

#define HELP_vmstat "usage: vmstat [-n] [DELAY [COUNT]]\n\nPrint virtual memory statistics, repeating each DELAY seconds, COUNT times.\n(With no DELAY, prints one line. With no COUNT, repeats until killed.)\n\nShow processes running and blocked, kilobytes swapped, free, buffered, and\ncached, kilobytes swapped in and out per second, file disk blocks input and\noutput per second, interrupts and context switches per second, percent\nof CPU time spent running user code, system code, idle, and awaiting I/O.\nFirst line is since system started, later lines are since last line.\n\n-n	Display the header only once"

#define HELP_vconfig "usage: vconfig COMMAND [OPTIONS]\n\nCreate and remove virtual ethernet devices\n\nadd             [interface-name] [vlan_id]\nrem             [vlan-name]\nset_flag        [interface-name] [flag-num]       [0 | 1]\nset_egress_map  [vlan-name]      [skb_priority]   [vlan_qos]\nset_ingress_map [vlan-name]      [skb_priority]   [vlan_qos]\nset_name_type   [name-type]"

#define HELP_uuidgen "usage: uuidgen\n\nCreate and print a new RFC4122 random UUID."

#define HELP_usleep "usage: usleep MICROSECONDS\n\nPause for MICROSECONDS microseconds."

#define HELP_uptime "usage: uptime [-ps]\n\nTell the current time, how long the system has been running, the number\nof users, and the system load averages for the past 1, 5 and 15 minutes.\n\n-p	Pretty (human readable) uptime\n-s	Since when has the system been up?"

#define HELP_truncate "usage: truncate [-c] -s SIZE file...\n\nSet length of file(s), extending sparsely if necessary.\n\n-c	Don't create file if it doesn't exist\n-s	New size (with optional prefix and suffix)\n\nSIZE prefix: + add, - subtract, < shrink to, > expand to,\n             / multiple rounding down, % multiple rounding up\nSIZE suffix: k=1024, m=1024^2, g=1024^3, t=1024^4, p=1024^5, e=1024^6"

#define HELP_timeout "usage: timeout [-k DURATION] [-s SIGNAL] DURATION COMMAND...\n\nRun command line as a child process, sending child a signal if the\ncommand doesn't exit soon enough.\n\nDURATION can be a decimal fraction. An optional suffix can be \"m\"\n(minutes), \"h\" (hours), \"d\" (days), or \"s\" (seconds, the default).\n\n-s	Send specified signal (default TERM)\n-k	Send KILL signal if child still running this long after first signal\n-v	Verbose\n--foreground       Don't create new process group\n--preserve-status  Exit with the child's exit status"

#define HELP_taskset "usage: taskset [-ap] [mask] [PID | cmd [args...]]\n\nLaunch a new task which may only run on certain processors, or change\nthe processor affinity of an existing PID.\n\nMask is a hex string where each bit represents a processor the process\nis allowed to run on. PID without a mask displays existing affinity.\n\n-p	Set/get the affinity of given PID instead of a new command\n-a	Set/get the affinity of all threads of the PID"

#define HELP_nproc "usage: nproc [--all]\n\nPrint number of processors.\n\n--all	Show all processors, not just ones this task can run on"

#define HELP_tac "usage: tac [FILE...]\n\nOutput lines in reverse order."

#define HELP_sysctl "usage: sysctl [-aAeNnqw] [-p [FILE] | KEY[=VALUE]...]\n\nRead/write system control data (under /proc/sys).\n\n-a,A	Show all values\n-e	Don't warn about unknown keys\n-N	Don't print key values\n-n	Don't print key names\n-p	Read values from FILE (default /etc/sysctl.conf)\n-q	Don't show value after write\n-w	Only write values (object to reading)"

#define HELP_switch_root "usage: switch_root [-c /dev/console] NEW_ROOT NEW_INIT...\n\nUse from PID 1 under initramfs to free initramfs, chroot to NEW_ROOT,\nand exec NEW_INIT.\n\n-c	Redirect console to device in NEW_ROOT\n-h	Hang instead of exiting on failure (avoids kernel panic)"

#define HELP_swapon "usage: swapon [-d] [-p priority] filename\n\nEnable swapping on a given device/file.\n\n-d	Discard freed SSD pages\n-p	Priority (highest priority areas allocated first)"

#define HELP_swapoff "usage: swapoff swapregion\n\nDisable swapping on a given swapregion."

#define HELP_stat "usage: stat [-tfL] [-c FORMAT] FILE...\n\nDisplay status of files or filesystems.\n\n-c	Output specified FORMAT string instead of default\n-f	Display filesystem status instead of file status\n-L	Follow symlinks\n-t	terse (-c \"%n %s %b %f %u %g %D %i %h %t %T %X %Y %Z %o\")\n	      (with -f = -c \"%n %i %l %t %s %S %b %f %a %c %d\")\n\nThe valid format escape sequences for files:\n%a  Access bits (octal) |%A  Access bits (flags)|%b  Size/512\n%B  Bytes per %b (512)  |%C  Security context   |%d  Device ID (dec)\n%D  Device ID (hex)     |%f  All mode bits (hex)|%F  File type\n%g  Group ID            |%G  Group name         |%h  Hard links\n%i  Inode               |%m  Mount point        |%n  Filename\n%N  Long filename       |%o  I/O block size     |%s  Size (bytes)\n%t  Devtype major (hex) |%T  Devtype minor (hex)|%u  User ID\n%U  User name           |%x  Access time        |%X  Access unix time\n%y  Modification time   |%Y  Mod unix time      |%z  Creation time\n%Z  Creation unix time\n\nThe valid format escape sequences for filesystems:\n%a  Available blocks    |%b  Total blocks       |%c  Total inodes\n%d  Free inodes         |%f  Free blocks        |%i  File system ID\n%l  Max filename length |%n  File name          |%s  Fragment size\n%S  Best transfer size  |%t  FS type (hex)      |%T  FS type (driver name)"

#define HELP_shred "usage: shred [-fuz] [-n COUNT] [-s SIZE] FILE...\n\nSecurely delete a file by overwriting its contents with random data.\n\n-f		Force (chmod if necessary)\n-n COUNT	Random overwrite iterations (default 1)\n-o OFFSET	Start at OFFSET\n-s SIZE		Use SIZE instead of detecting file size\n-u		Unlink (actually delete file when done)\n-x		Use exact size (default without -s rounds up to next 4k)\n-z		Zero at end\n\nNote: data journaling filesystems render this command useless, you must\noverwrite all free space (fill up disk) to erase old data on those."

#define HELP_setsid "usage: setsid [-cdw] command [args...]\n\nRun process in a new session.\n\n-d	Detach from tty\n-c	Control tty (become foreground process & receive keyboard signals)\n-w	Wait for child (and exit with its status)"

#define HELP_setfattr "usage: setfattr [-h] [-x|-n NAME] [-v VALUE] FILE...\n\nWrite POSIX extended attributes.\n\n-h	Do not dereference symlink\n-n	Set given attribute\n-x	Remove given attribute\n-v	Set value for attribute -n (default is empty)"

#define HELP_rmmod "usage: rmmod [-wf] [MODULE]\n\nUnload the module named MODULE from the Linux kernel.\n-f	Force unload of a module\n-w	Wait until the module is no longer used"

#define HELP_rev "usage: rev [FILE...]\n\nOutput each line reversed, when no files are given stdin is used."

#define HELP_reset "usage: reset\n\nReset the terminal."

#define HELP_reboot "usage: reboot/halt/poweroff [-fn]\n\nRestart, halt or powerdown the system.\n\n-f	Don't signal init\n-n	Don't sync before stopping the system"

#define HELP_realpath "usage: realpath FILE...\n\nDisplay the canonical absolute pathname"

#define HELP_readlink "usage: readlink FILE...\n\nWith no options, show what symlink points to, return error if not symlink.\n\nOptions for producing canonical paths (all symlinks/./.. resolved):\n\n-e	Canonical path to existing entry (fail if missing)\n-f	Full path (fail if directory missing)\n-m	Ignore missing entries, show where it would be\n-n	No trailing newline\n-q	Quiet (no output, just error code)"

#define HELP_readahead "usage: readahead FILE...\n\nPreload files into disk cache."

#define HELP_pwdx "usage: pwdx PID...\n\nPrint working directory of processes listed on command line."

#define HELP_printenv "usage: printenv [-0] [env_var...]\n\nPrint environment variables.\n\n-0	Use \\0 as delimiter instead of \\n"

#define HELP_pmap "usage: pmap [-xq] [pids...]\n\nReport the memory map of a process or processes.\n\n-x	Show the extended format\n-q	Do not display some header/footer lines"

#define HELP_pivot_root "usage: pivot_root OLD NEW\n\nSwap OLD and NEW filesystems (as if by simultaneous mount --move), and\nmove all processes with chdir or chroot under OLD into NEW (including\nkernel threads) so OLD may be unmounted.\n\nThe directory NEW must exist under OLD. This doesn't work on initramfs,\nwhich can't be moved (about the same way PID 1 can't be killed; see\nswitch_root instead)."

#define HELP_partprobe "usage: partprobe DEVICE...\n\nTell the kernel about partition table changes\n\nAsk the kernel to re-read the partition table on the specified devices."

#define HELP_oneit "usage: oneit [-p] [-c /dev/tty0] command [...]\n\nSimple init program that runs a single supplied command line with a\ncontrolling tty (so CTRL-C can kill it).\n\n-c	Which console device to use (/dev/console doesn't do CTRL-C, etc)\n-p	Power off instead of rebooting when command exits\n-r	Restart child when it exits\n-3	Write 32 bit PID of each exiting reparented process to fd 3 of child\n	(Blocking writes, child must read to avoid eventual deadlock.)\n\nSpawns a single child process (because PID 1 has signals blocked)\nin its own session, reaps zombies until the child exits, then\nreboots the system (or powers off with -p, or restarts the child with -r).\n\nResponds to SIGUSR1 by halting the system, SIGUSR2 by powering off,\nand SIGTERM or SIGINT reboot."

#define HELP_nsenter "usage: nsenter [-t pid] [-F] [-i] [-m] [-n] [-p] [-u] [-U] COMMAND...\n\nRun COMMAND in an existing (set of) namespace(s).\n\n-t	PID to take namespaces from    (--target)\n-F	don't fork, even if -p is used (--no-fork)\n\nThe namespaces to switch are:\n\n-i	SysV IPC: message queues, semaphores, shared memory (--ipc)\n-m	Mount/unmount tree (--mount)\n-n	Network address, sockets, routing, iptables (--net)\n-p	Process IDs and init, will fork unless -F is used (--pid)\n-u	Host and domain names (--uts)\n-U	UIDs, GIDs, capabilities (--user)\n\nIf -t isn't specified, each namespace argument must provide a path\nto a namespace file, ala \"-i=/proc/$PID/ns/ipc\""

#define HELP_unshare "usage: unshare [-imnpuUr] COMMAND...\n\nCreate new container namespace(s) for this process and its children, so\nsome attribute is not shared with the parent process.\n\n-f	Fork command in the background (--fork)\n-i	SysV IPC (message queues, semaphores, shared memory) (--ipc)\n-m	Mount/unmount tree (--mount)\n-n	Network address, sockets, routing, iptables (--net)\n-p	Process IDs and init (--pid)\n-r	Become root (map current euid/egid to 0/0, implies -U) (--map-root-user)\n-u	Host and domain names (--uts)\n-U	UIDs, GIDs, capabilities (--user)\n\nA namespace allows a set of processes to have a different view of the\nsystem than other sets of processes."

#define HELP_nbd_client "usage: nbd-client [-ns] HOST PORT DEVICE\n\n-n	Do not fork into background\n-s	nbd swap support (lock server into memory)"

#define HELP_mountpoint "usage: mountpoint [-qd] DIR\n       mountpoint [-qx] DEVICE\n\nCheck whether the directory or device is a mountpoint.\n\n-q	Be quiet, return zero if directory is a mountpoint\n-d	Print major/minor device number of the directory\n-x	Print major/minor device number of the block device"

#define HELP_modinfo "usage: modinfo [-0] [-b basedir] [-k kernel] [-F field] [module|file...]\n\nDisplay module fields for modules specified by name or .ko path.\n\n-F  Only show the given field\n-0  Separate fields with NUL rather than newline\n-b  Use <basedir> as root for /lib/modules/\n-k  Look in given directory under /lib/modules/"

#define HELP_mkswap "usage: mkswap [-L LABEL] DEVICE\n\nSet up a Linux swap area on a device or file."

#define HELP_mkpasswd "usage: mkpasswd [-P FD] [-m TYPE] [-S SALT] [PASSWORD] [SALT]\n\nCrypt PASSWORD using crypt(3)\n\n-P FD	Read password from file descriptor FD\n-m TYPE	Encryption method (des, md5, sha256, or sha512; default is des)\n-S SALT"

#define HELP_mix "usage: mix [-d DEV] [-c CHANNEL] [-l VOL] [-r RIGHT]\n\nList OSS sound channels (module snd-mixer-oss), or set volume(s).\n\n-c CHANNEL	Set/show volume of CHANNEL (default first channel found)\n-d DEV		Device node (default /dev/mixer)\n-l VOL		Volume level\n-r RIGHT	Volume of right stereo channel (with -r, -l sets left volume)"

#define HELP_mcookie "usage: mcookie [-vV]\n\nGenerate a 128-bit strong random number.\n\n-v  show entropy source (verbose)\n-V  show version"

#define HELP_makedevs "usage: makedevs [-d device_table] rootdir\n\nCreate a range of special files as specified in a device table.\n\n-d	File containing device table (default reads from stdin)\n\nEach line of the device table has the fields:\n<name> <type> <mode> <uid> <gid> <major> <minor> <start> <increment> <count>\nWhere name is the file name, and type is one of the following:\n\nb	Block device\nc	Character device\nd	Directory\nf	Regular file\np	Named pipe (fifo)\n\nOther fields specify permissions, user and group id owning the file,\nand additional fields for device special files. Use '-' for blank entries,\nunspecified fields are treated as '-'."

#define HELP_lsusb "usage: lsusb\n\nList USB hosts/devices."

#define HELP_lspci_text "usage: lspci [-n] [-i FILE ]\n\n-n	Numeric output (repeat for readable and numeric)\n-i	PCI ID database (default /usr/share/misc/pci.ids)"

#define HELP_lspci "usage: lspci [-ekm]\n\nList PCI devices.\n\n-e	Print all 6 digits in class\n-k	Print kernel driver\n-m	Machine parseable format"

#define HELP_lsmod "usage: lsmod\n\nDisplay the currently loaded modules, their sizes and their dependencies."

#define HELP_chattr "usage: chattr [-R] [-+=AacDdijsStTu] [-p PROJID] [-v VERSION] [FILE...]\n\nChange file attributes on a Linux file system.\n\n-R	Recurse\n-p	Set the file's project number\n-v	Set the file's version/generation number\n\nOperators:\n  '-' Remove attributes\n  '+' Add attributes\n  '=' Set attributes\n\nAttributes:\n  A  No atime                     a  Append only\n  C  No COW                       c  Compression\n  D  Synchronous dir updates      d  No dump\n  E  Encrypted                    e  Extents\n  F  Case-insensitive (casefold)\n  I  Indexed directory            i  Immutable\n  j  Journal data\n  N  Inline data in inode\n  P  Project hierarchy\n  S  Synchronous file updates     s  Secure delete\n  T  Top of dir hierarchy         t  No tail-merging\n  u  Allow undelete\n  V  Verity"

#define HELP_lsattr "usage: lsattr [-Radlpv] [FILE...]\n\nList file attributes on a Linux file system.\nFlag letters are defined in chattr help.\n\n-R	Recursively list attributes of directories and their contents\n-a	List all files in directories, including files that start with '.'\n-d	List directories like other files, rather than listing their contents\n-l	List long flag names\n-p	List the file's project number\n-v	List the file's version/generation number"

#define HELP_losetup "usage: losetup [-cdrs] [-o OFFSET] [-S SIZE] {-d DEVICE...|-j FILE|-af|{DEVICE FILE}}\n\nAssociate a loopback device with a file, or show current file (if any)\nassociated with a loop device.\n\nInstead of a device:\n-a	Iterate through all loopback devices\n-f	Find first unused loop device (may create one)\n-j FILE	Iterate through all loopback devices associated with FILE\n\nexisting:\n-c	Check capacity (file size changed)\n-d DEV	Detach loopback device\n-D	Detach all loopback devices\n\nnew:\n-s	Show device name (alias --show)\n-o OFF	Start association at offset OFF into FILE\n-r	Read only\n-S SIZE	Limit SIZE of loopback association (alias --sizelimit)"

#define HELP_login "usage: login [-p] [-h host] [-f USERNAME] [USERNAME]\n\nLog in as a user, prompting for username and password if necessary.\n\n-p	Preserve environment\n-h	The name of the remote host for this login\n-f	login as USERNAME without authentication"

#define HELP_iorenice "usage: iorenice PID [CLASS] [PRIORITY]\n\nDisplay or change I/O priority of existing process. CLASS can be\n\"rt\" for realtime, \"be\" for best effort, \"idle\" for only when idle, or\n\"none\" to leave it alone. PRIORITY can be 0-7 (0 is highest, default 4)."

#define HELP_ionice "usage: ionice [-t] [-c CLASS] [-n LEVEL] [COMMAND...|-p PID]\n\nChange the I/O scheduling priority of a process. With no arguments\n(or just -p), display process' existing I/O class/priority.\n\n-c	CLASS = 1-3: 1(realtime), 2(best-effort, default), 3(when-idle)\n-n	LEVEL = 0-7: (0 is highest priority, default = 5)\n-p	Affect existing PID instead of spawning new child\n-t	Ignore failure to set I/O priority\n\nSystem default iopriority is generally -c 2 -n 4."

#define HELP_insmod "usage: insmod MODULE [MODULE_OPTIONS]\n\nLoad the module named MODULE passing options if given."

#define HELP_inotifyd "usage: inotifyd PROG FILE[:MASK] ...\n\nWhen a filesystem event matching MASK occurs to a FILE, run PROG as:\n\n  PROG EVENTS FILE [DIRFILE]\n\nIf PROG is \"-\" events are sent to stdout.\n\nThis file is:\n  a  accessed    c  modified    e  metadata change  w  closed (writable)\n  r  opened      D  deleted     M  moved            0  closed (unwritable)\n  u  unmounted   o  overflow    x  unwatchable\n\nA file in this directory is:\n  m  moved in    y  moved out   n  created          d  deleted\n\nWhen x event happens for all FILEs, inotifyd exits (after waiting for PROG)."

#define HELP_i2cset "usage: i2cset [-fy] BUS CHIP ADDR VALUE... MODE\n\nWrite an i2c register. MODE is b for byte, w for 16-bit word, i for I2C block.\n\n-f	Force access to busy devices\n-y	Answer \"yes\" to confirmation prompts (for script use)"

#define HELP_i2cget "usage: i2cget [-fy] BUS CHIP ADDR\n\nRead an i2c register.\n\n-f	Force access to busy devices\n-y	Answer \"yes\" to confirmation prompts (for script use)"

#define HELP_i2cdump "usage: i2cdump [-fy] BUS CHIP\n\nDump i2c registers.\n\n-f	Force access to busy devices\n-y	Answer \"yes\" to confirmation prompts (for script use)"

#define HELP_i2cdetect "usage: i2cdetect [-ary] BUS [FIRST LAST]\nusage: i2cdetect -F BUS\nusage: i2cdetect -l\n\nDetect i2c devices.\n\n-a	All addresses (0x00-0x7f rather than 0x03-0x77)\n-F	Show functionality\n-l	List all buses\n-r	Probe with SMBus Read Byte\n-y	Answer \"yes\" to confirmation prompts (for script use)"

#define HELP_hwclock "usage: hwclock [-rswtluf]\n\nGet/set the hardware clock.\n\n-f FILE	Use specified device file instead of /dev/rtc (--rtc)\n-l	Hardware clock uses localtime (--localtime)\n-r	Show hardware clock time (--show)\n-s	Set system time from hardware clock (--hctosys)\n-t	Set the system time based on the current timezone (--systz)\n-u	Hardware clock uses UTC (--utc)\n-w	Set hardware clock from system time (--systohc)"

#define HELP_hexedit "usage: hexedit FILENAME\n\nHexadecimal file editor. All changes are written to disk immediately.\n\n-r	Read only (display but don't edit)\n\nKeys:\nArrows        Move left/right/up/down by one line/column\nPg Up/Pg Dn   Move up/down by one page\n0-9, a-f      Change current half-byte to hexadecimal value\nu             Undo\nq/^c/^d/<esc> Quit"

#define HELP_help "usage: help [-ahu] [COMMAND]\n\n-a	All commands\n-u	Usage only\n-h	HTML output\n\nShow usage information for toybox commands.\nRun \"toybox\" with no arguments for a list of available commands."

#define HELP_fsync "usage: fsync [-d] [FILE...]\n\nSynchronize a file's in-core state with storage device.\n\n-d	Avoid syncing metadata"

#define HELP_fsfreeze "usage: fsfreeze {-f | -u} MOUNTPOINT\n\nFreeze or unfreeze a filesystem.\n\n-f	Freeze\n-u	Unfreeze"

#define HELP_freeramdisk "usage: freeramdisk [RAM device]\n\nFree all memory allocated to specified ramdisk"

#define HELP_free "usage: free [-bkmgt]\n\nDisplay the total, free and used amount of physical memory and swap space.\n\n-bkmgt	Output units (default is bytes)\n-h	Human readable (K=1024)"

#define HELP_fmt "usage: fmt [-w WIDTH] [FILE...]\n\nReformat input to wordwrap at a given line length, preserving existing\nindentation level, writing to stdout.\n\n-w WIDTH	Maximum characters per line (default 75)"

#define HELP_flock "usage: flock [-sxun] fd\n\nManage advisory file locks.\n\n-s	Shared lock\n-x	Exclusive lock (default)\n-u	Unlock\n-n	Non-blocking: fail rather than wait for the lock"

#define HELP_fallocate "usage: fallocate [-l size] [-o offset] file\n\nTell the filesystem to allocate space for a file."

#define HELP_factor "usage: factor NUMBER...\n\nFactor integers."

#define HELP_eject "usage: eject [-stT] [DEVICE]\n\nEject DEVICE or default /dev/cdrom\n\n-s	SCSI device\n-t	Close tray\n-T	Open/close tray (toggle)"

#define HELP_unix2dos "usage: unix2dos [FILE...]\n\nConvert newline format from unix \"\\n\" to dos \"\\r\\n\".\nIf no files listed copy from stdin, \"-\" is a synonym for stdin."

#define HELP_dos2unix "usage: dos2unix [FILE...]\n\nConvert newline format from dos \"\\r\\n\" to unix \"\\n\".\nIf no files listed copy from stdin, \"-\" is a synonym for stdin."

#define HELP_devmem "usage: devmem ADDR [WIDTH [DATA]]\n\nRead/write physical address via /dev/mem.\n\nWIDTH is 1, 2, 4, or 8 bytes (default 4)."

#define HELP_count "usage: count\n\nCopy stdin to stdout, displaying simple progress indicator to stderr."

#define HELP_clear "Clear the screen."

#define HELP_chvt "usage: chvt N\n\nChange to virtual terminal number N. (This only works in text mode.)\n\nVirtual terminals are the Linux VGA text mode displays, ordinarily\nswitched between via alt-F1, alt-F2, etc. Use ctrl-alt-F1 to switch\nfrom X to a virtual terminal, and alt-F6 (or F7, or F8) to get back."

#define HELP_chrt "usage: chrt [-Rmofrbi] {-p PID [PRIORITY] | [PRIORITY COMMAND...]}\n\nGet/set a process' real-time scheduling policy and priority.\n\n-p	Set/query given pid (instead of running COMMAND)\n-R	Set SCHED_RESET_ON_FORK\n-m	Show min/max priorities available\n\nSet policy (default -r):\n\n  -o  SCHED_OTHER    -f  SCHED_FIFO    -r  SCHED_RR\n  -b  SCHED_BATCH    -i  SCHED_IDLE"

#define HELP_chroot "usage: chroot NEWROOT [COMMAND [ARG...]]\n\nRun command within a new root directory. If no command, run /bin/sh."

#define HELP_chcon "usage: chcon [-hRv] CONTEXT FILE...\n\nChange the SELinux security context of listed file[s].\n\n-h	Change symlinks instead of what they point to\n-R	Recurse into subdirectories\n-v	Verbose"

#define HELP_bzcat "usage: bzcat [FILE...]\n\nDecompress listed files to stdout. Use stdin if no files listed."

#define HELP_bunzip2 "usage: bunzip2 [-cftkv] [FILE...]\n\nDecompress listed files (file.bz becomes file) deleting archive file(s).\nRead from stdin if no files listed.\n\n-c	Force output to stdout\n-f	Force decompression (if FILE doesn't end in .bz, replace original)\n-k	Keep input files (-c and -t imply this)\n-t	Test integrity\n-v	Verbose"

#define HELP_blockdev "usage: blockdev --OPTION... BLOCKDEV...\n\nCall ioctl(s) on each listed block device\n\n--setro		Set read only\n--setrw		Set read write\n--getro		Get read only\n--getss		Get sector size\n--getbsz	Get block size\n--setbsz BYTES	Set block size\n--getsz		Get device size in 512-byte sectors\n--getsize	Get device size in sectors (deprecated)\n--getsize64	Get device size in bytes\n--getra		Get readahead in 512-byte sectors\n--setra SECTORS	Set readahead\n--flushbufs	Flush buffers\n--rereadpt	Reread partition table"

#define HELP_fstype "usage: fstype DEV...\n\nPrint type of filesystem on a block device or image."

#define HELP_blkid "usage: blkid [-s TAG] [-UL] DEV...\n\nPrint type, label and UUID of filesystem on a block device or image.\n\n-U	Show UUID only (or device with that UUID)\n-L	Show LABEL only (or device with that LABEL)\n-s TAG	Only show matching tags (default all)"

#define HELP_base64 "usage: base64 [-di] [-w COLUMNS] [FILE...]\n\nEncode or decode in base64.\n\n-d	Decode\n-i	Ignore non-alphabetic characters\n-w	Wrap output at COLUMNS (default 76 or 0 for no wrap)"

#define HELP_ascii "usage: ascii\n\nDisplay ascii character set."

#define HELP_acpi "usage: acpi [-abctV]\n\nShow status of power sources and thermal devices.\n\n-a	Show power adapters\n-b	Show batteries\n-c	Show cooling device state\n-t	Show temperatures\n-V	Show everything"

#define HELP_xzcat "usage: xzcat [filename...]\n\nDecompress listed files to stdout. Use stdin if no files listed."

#define HELP_wget "usage: wget -O filename URL\n-O filename: specify output filename\nURL: uniform resource location, FTP/HTTP only, not HTTPS\n\nexamples:\n  wget -O index.html http://www.example.com\n  wget -O sample.jpg ftp://ftp.example.com:21/sample.jpg"

#define HELP_vi "usage: vi [-s script] FILE\n-s script: run script file\nVisual text editor. Predates the existence of standardized cursor keys,\nso the controls are weird and historical."

#define HELP_userdel "usage: userdel [-r] USER\nusage: deluser [-r] USER\n\nDelete USER from the SYSTEM\n\n-r	remove home directory"

#define HELP_useradd "usage: useradd [-SDH] [-h DIR] [-s SHELL] [-G GRP] [-g NAME] [-u UID] USER [GROUP]\n\nCreate new user, or add USER to GROUP\n\n-D       Don't assign a password\n-g NAME  Real name\n-G GRP   Add user to existing group\n-h DIR   Home directory\n-H       Don't create home directory\n-s SHELL Login shell\n-S       Create a system user\n-u UID   User id"

#define HELP_traceroute "usage: traceroute [-46FUIldnvr] [-f 1ST_TTL] [-m MAXTTL] [-p PORT] [-q PROBES]\n[-s SRC_IP] [-t TOS] [-w WAIT_SEC] [-g GATEWAY] [-i IFACE] [-z PAUSE_MSEC] HOST [BYTES]\n\ntraceroute6 [-dnrv] [-m MAXTTL] [-p PORT] [-q PROBES][-s SRC_IP] [-t TOS] [-w WAIT_SEC]\n  [-i IFACE] HOST [BYTES]\n\nTrace the route to HOST\n\n-4,-6 Force IP or IPv6 name resolution\n-F    Set the don't fragment bit (supports IPV4 only)\n-U    Use UDP datagrams instead of ICMP ECHO (supports IPV4 only)\n-I    Use ICMP ECHO instead of UDP datagrams (supports IPV4 only)\n-l    Display the TTL value of the returned packet (supports IPV4 only)\n-d    Set SO_DEBUG options to socket\n-n    Print numeric addresses\n-v    verbose\n-r    Bypass routing tables, send directly to HOST\n-m    Max time-to-live (max number of hops)(RANGE 1 to 255)\n-p    Base UDP port number used in probes(default 33434)(RANGE 1 to 65535)\n-q    Number of probes per TTL (default 3)(RANGE 1 to 255)\n-s    IP address to use as the source address\n-t    Type-of-service in probe packets (default 0)(RANGE 0 to 255)\n-w    Time in seconds to wait for a response (default 3)(RANGE 0 to 86400)\n-g    Loose source route gateway (8 max) (supports IPV4 only)\n-z    Pause Time in ms (default 0)(RANGE 0 to 86400) (supports IPV4 only)\n-f    Start from the 1ST_TTL hop (instead from 1)(RANGE 1 to 255) (supports IPV4 only)\n-i    Specify a network interface to operate with"

#define HELP_tr "usage: tr [-cds] SET1 [SET2]\n\nTranslate, squeeze, or delete characters from stdin, writing to stdout\n\n-c/-C  Take complement of SET1\n-d     Delete input characters coded SET1\n-s     Squeeze multiple output characters of SET2 into one character"

#define HELP_tftpd "usage: tftpd [-cr] [-u USER] [DIR]\n\nTransfer file from/to tftp server.\n\n-r	read only\n-c	Allow file creation via upload\n-u	run as USER\n-l	Log to syslog (inetd mode requires this)"

#define HELP_tftp "usage: tftp [OPTIONS] HOST [PORT]\n\nTransfer file from/to tftp server.\n\n-l FILE Local FILE\n-r FILE Remote FILE\n-g    Get file\n-p    Put file\n-b SIZE Transfer blocks of SIZE octets(8 <= SIZE <= 65464)"

#define HELP_telnetd "Handle incoming telnet connections\n\n-l LOGIN  Exec LOGIN on connect\n-f ISSUE_FILE Display ISSUE_FILE instead of /etc/issue\n-K Close connection as soon as login exits\n-p PORT   Port to listen on\n-b ADDR[:PORT]  Address to bind to\n-F Run in foreground\n-i Inetd mode\n-w SEC    Inetd 'wait' mode, linger time SEC\n-S Log to syslog (implied by -i or without -F and -w)"

#define HELP_telnet "usage: telnet HOST [PORT]\n\nConnect to telnet server"

#define HELP_tcpsvd "usage: tcpsvd [-hEv] [-c N] [-C N[:MSG]] [-b N] [-u User] [-l Name] IP Port Prog\nusage: udpsvd [-hEv] [-c N] [-u User] [-l Name] IP Port Prog\n\nCreate TCP/UDP socket, bind to IP:PORT and listen for incoming connection.\nRun PROG for each connection.\n\nIP            IP to listen on, 0 = all\nPORT          Port to listen on\nPROG ARGS     Program to run\n-l NAME       Local hostname (else looks up local hostname in DNS)\n-u USER[:GRP] Change to user/group after bind\n-c N          Handle up to N (> 0) connections simultaneously\n-b N          (TCP Only) Allow a backlog of approximately N TCP SYNs\n-C N[:MSG]    (TCP Only) Allow only up to N (> 0) connections from the same IP\n              New connections from this IP address are closed\n              immediately. MSG is written to the peer before close\n-h            Look up peer's hostname\n-E            Don't set up environment variables\n-v            Verbose"

#define HELP_syslogd "usage: syslogd  [-a socket] [-O logfile] [-f config file] [-m interval]\n                [-p socket] [-s SIZE] [-b N] [-R HOST] [-l N] [-nSLKD]\n\nSystem logging utility\n\n-a      Extra unix socket for listen\n-O FILE Default log file <DEFAULT: /var/log/messages>\n-f FILE Config file <DEFAULT: /etc/syslog.conf>\n-p      Alternative unix domain socket <DEFAULT : /dev/log>\n-n      Avoid auto-backgrounding\n-S      Smaller output\n-m MARK interval <DEFAULT: 20 minutes> (RANGE: 0 to 71582787)\n-R HOST Log to IP or hostname on PORT (default PORT=514/UDP)\"\n-L      Log locally and via network (default is network only if -R)\"\n-s SIZE Max size (KB) before rotation (default:200KB, 0=off)\n-b N    rotated logs to keep (default:1, max=99, 0=purge)\n-K      Log to kernel printk buffer (use dmesg to read it)\n-l N    Log only messages more urgent than prio(default:8 max:8 min:1)\n-D      Drop duplicates"

#define HELP_sulogin "usage: sulogin [-t time] [tty]\n\nSingle User Login.\n-t	Default Time for Single User Login"

#define HELP_stty "usage: stty [-ag] [-F device] SETTING...\n\nGet/set terminal configuration.\n\n-F	Open device instead of stdin\n-a	Show all current settings (default differences from \"sane\")\n-g	Show all current settings usable as input to stty\n\nSpecial characters (syntax ^c or undef): intr quit erase kill eof eol eol2\nswtch start stop susp rprnt werase lnext discard\n\nControl/input/output/local settings as shown by -a, '-' prefix to disable\n\nCombo settings: cooked/raw, evenp/oddp/parity, nl, ek, sane\n\nN	set input and output speed (ispeed N or ospeed N for just one)\ncols N	set number of columns\nrows N	set number of rows\nline N	set line discipline\nmin N	set minimum chars per read\ntime N	set read timeout\nspeed	show speed only\nsize	show size only"

#define HELP_exit "usage: exit [status]\n\nExit shell.  If no return value supplied on command line, use value\nof most recent command, or 0 if none."

#define HELP_cd "usage: cd [-PL] [path]\n\nChange current directory.  With no arguments, go $HOME.\n\n-P	Physical path: resolve symlinks in path\n-L	Local path: .. trims directories off $PWD (default)"

#define HELP_sh "usage: sh [-c command] [script]\n\nCommand shell.  Runs a shell script, or reads input interactively\nand responds to it.\n\n-c	command line to execute\n-i	interactive mode (default when STDIN is a tty)"

#define HELP_route "usage: route [-ne] [-A [46]] [add|del TARGET [OPTIONS]]\n\nDisplay, add or delete network routes in the \"Forwarding Information Base\".\n\n-n	Show numerical addresses (no DNS lookups)\n-e	display netstat fields\n\nRouting means sending packets out a network interface to an address.\nThe kernel can tell where to send packets one hop away by examining each\ninterface's address and netmask, so the most common use of this command\nis to identify a \"gateway\" that forwards other traffic.\n\nAssigning an address to an interface automatically creates an appropriate\nnetwork route (\"ifconfig eth0 10.0.2.15/8\" does \"route add 10.0.0.0/8 eth0\"\nfor you), although some devices (such as loopback) won't show it in the\ntable. For machines more than one hop away, you need to specify a gateway\n(ala \"route add default gw 10.0.2.2\").\n\nThe address \"default\" is a wildcard address (0.0.0.0/0) matching all\npackets without a more specific route.\n\nAvailable OPTIONS include:\nreject   - blocking route (force match failure)\ndev NAME - force packets out this interface (ala \"eth0\")\nnetmask  - old way of saying things like ADDR/24\ngw ADDR  - forward packets to gateway ADDR"

#define HELP_readelf "usage: readelf [-adhlnSsW] [-p SECTION] [-x SECTION] [file...]\n\nDisplays information about ELF files.\n\n-a	Equivalent to -dhlnSs\n-d	Show dynamic section\n-h	Show ELF header\n-l	Show program headers\n-n	Show notes\n-p S	Dump strings found in named/numbered section\n-S	Show section headers\n-s	Show symbol tables (.dynsym and .symtab)\n-W	Don't truncate fields (default in toybox)\n-x S	Hex dump of named/numbered section\n\n--dyn-syms	Show just .dynsym symbol table"

#define HELP_deallocvt "usage: deallocvt [N]\n\nDeallocate unused virtual terminal /dev/ttyN, or all unused consoles."

#define HELP_openvt "usage: openvt [-c N] [-sw] [command [command_options]]\n\nstart a program on a new virtual terminal (VT)\n\n-c N  Use VT N\n-s    Switch to new VT\n-w    Wait for command to exit\n\nif -sw used together, switch back to originating VT when command completes"

#define HELP_more "usage: more [FILE...]\n\nView FILE(s) (or stdin) one screenfull at a time."

#define HELP_modprobe "usage: modprobe [-alrqvsDb] [-d DIR] MODULE [symbol=value][...]\n\nmodprobe utility - inserts modules and dependencies.\n\n-a  Load multiple MODULEs\n-d  Load modules from DIR, option may be used multiple times\n-l  List (MODULE is a pattern)\n-r  Remove MODULE (stacks) or do autoclean\n-q  Quiet\n-v  Verbose\n-s  Log to syslog\n-D  Show dependencies\n-b  Apply blacklist to module names too"

#define HELP_mke2fs_extended "usage: mke2fs [-E stride=###] [-O option[,option]]\n\n-E stride= Set RAID stripe size (in blocks)\n-O [opts]  Specify fewer ext2 option flags (for old kernels)\n           All of these are on by default (as appropriate)\n   none         Clear default options (all but journaling)\n   dir_index    Use htree indexes for large directories\n   filetype     Store file type info in directory entry\n   has_journal  Set by -j\n   journal_dev  Set by -J device=XXX\n   sparse_super Don't allocate huge numbers of redundant superblocks"

#define HELP_mke2fs_label "usage: mke2fs [-L label] [-M path] [-o string]\n\n-L         Volume label\n-M         Path to mount point\n-o         Created by"

#define HELP_mke2fs_gen "usage: gene2fs [options] device filename\n\nThe [options] are the same as mke2fs."

#define HELP_mke2fs_journal "usage: mke2fs [-j] [-J size=###,device=XXX]\n\n-j         Create journal (ext3)\n-J         Journal options\n           size: Number of blocks (1024-102400)\n           device: Specify an external journal"

#define HELP_mke2fs "usage: mke2fs [-Fnq] [-b ###] [-N|i ###] [-m ###] device\n\nCreate an ext2 filesystem on a block device or filesystem image.\n\n-F         Force to run on a mounted device\n-n         Don't write to device\n-q         Quiet (no output)\n-b size    Block size (1024, 2048, or 4096)\n-N inodes  Allocate this many inodes\n-i bytes   Allocate one inode for every XXX bytes of device\n-m percent Reserve this percent of filesystem space for root user"

#define HELP_mdev_conf "The mdev config file (/etc/mdev.conf) contains lines that look like:\nhd[a-z][0-9]* 0:3 660\n(sd[a-z]) root:disk 660 =usb_storage\n\nEach line must contain three whitespace separated fields. The first\nfield is a regular expression matching one or more device names,\nthe second and third fields are uid:gid and file permissions for\nmatching devices. Fourth field is optional. It could be used to change\ndevice name (prefix '='), path (prefix '=' and postfix '/') or create a\nsymlink (prefix '>')."

#define HELP_mdev "usage: mdev [-s]\n\nCreate devices in /dev using information from /sys.\n\n-s	Scan all entries in /sys to populate /dev"

#define HELP_man "usage: man [-M PATH] [-k STRING] | [SECTION] COMMAND\n\nRead manual page for system command.\n\n-k	List pages with STRING in their short description\n-M	Override $MANPATH\n\nMan pages are divided into 8 sections:\n1 commands      2 system calls  3 library functions  4 /dev files\n5 file formats  6 games         7 miscellaneous      8 system management\n\nSections are searched in the order 1 8 3 2 5 4 6 7 unless you specify a\nsection. Each section has a page called \"intro\", and there's a global\nintroduction under \"man-pages\"."

#define HELP_lsof "usage: lsof [-lt] [-p PID1,PID2,...] [FILE...]\n\nList all open files belonging to all active processes, or processes using\nlisted FILE(s).\n\n-l	list uids numerically\n-p	for given comma-separated pids only (default all pids)\n-t	terse (pid only) output"

#define HELP_last "usage: last [-W] [-f FILE]\n\nShow listing of last logged in users.\n\n-W      Display the information without host-column truncation\n-f FILE Read from file FILE instead of /var/log/wtmp"

#define HELP_klogd "usage: klogd [-n] [-c N]\n\n-c  N   Print to console messages more urgent than prio N (1-8)\"\n-n    Run in foreground"

#define HELP_ipcs "usage: ipcs [[-smq] -i shmid] | [[-asmq] [-tcplu]]\n\n-i Show specific resource\nResource specification:\n-a All (default)\n-m Shared memory segments\n-q Message queues\n-s Semaphore arrays\nOutput format:\n-c Creator\n-l Limits\n-p Pid\n-t Time\n-u Summary"

#define HELP_ipcrm "usage: ipcrm [ [-q msqid] [-m shmid] [-s semid]\n          [-Q msgkey] [-M shmkey] [-S semkey] ... ]\n\n-mM Remove memory segment after last detach\n-qQ Remove message queue\n-sS Remove semaphore"

#define HELP_ip "usage: ip [ OPTIONS ] OBJECT { COMMAND }\n\nShow / manipulate routing, devices, policy routing and tunnels.\n\nwhere OBJECT := {address | link | route | rule | tunnel}\nOPTIONS := { -f[amily] { inet | inet6 | link } | -o[neline] }"

#define HELP_init "usage: init\n\nSystem V style init.\n\nFirst program to run (as PID 1) when the system comes up, reading\n/etc/inittab to determine actions."

#define HELP_host "usage: host [-av] [-t TYPE] NAME [SERVER]\n\nPerform DNS lookup on NAME, which can be a domain name to lookup,\nor an IPv4 dotted or IPv6 colon-separated address to reverse lookup.\nSERVER (if present) is the DNS server to use.\n\n-a	-v -t ANY\n-t TYPE	query records of type TYPE\n-v	verbose"

#define HELP_groupdel "usage: groupdel [USER] GROUP\n\nDelete a group or remove a user from a group"

#define HELP_groupadd "usage: groupadd [-S] [-g GID] [USER] GROUP\n\nAdd a group or add a user to a group\n\n  -g GID Group id\n  -S     Create a system group"

#define HELP_getty "usage: getty [OPTIONS] BAUD_RATE[,BAUD_RATE]... TTY [TERMTYPE]\n\n-h    Enable hardware RTS/CTS flow control\n-L    Set CLOCAL (ignore Carrier Detect state)\n-m    Get baud rate from modem's CONNECT status message\n-n    Don't prompt for login name\n-w    Wait for CR or LF before sending /etc/issue\n-i    Don't display /etc/issue\n-f ISSUE_FILE  Display ISSUE_FILE instead of /etc/issue\n-l LOGIN  Invoke LOGIN instead of /bin/login\n-t SEC    Terminate after SEC if no login name is read\n-I INITSTR  Send INITSTR before anything else\n-H HOST    Log HOST into the utmp file as the hostname"

#define HELP_getopt "usage: getopt [OPTIONS] [--] ARG...\n\nParse command-line options for use in shell scripts.\n\n-a	Allow long options starting with a single -.\n-l OPTS	Specify long options.\n-n NAME	Command name for error messages.\n-o OPTS	Specify short options.\n-T	Test whether this is a modern getopt.\n-u	Output options unquoted."

#define HELP_getfattr "usage: getfattr [-d] [-h] [-n NAME] FILE...\n\nRead POSIX extended attributes.\n\n-d	Show values as well as names\n-h	Do not dereference symbolic links\n-n	Show only attributes with the given name\n--only-values	Don't show names"

#define HELP_fsck "usage: fsck [-ANPRTV] [-C FD] [-t FSTYPE] [FS_OPTS] [BLOCKDEV]...\n\nCheck and repair filesystems\n\n-A      Walk /etc/fstab and check all filesystems\n-N      Don't execute, just show what would be done\n-P      With -A, check filesystems in parallel\n-R      With -A, skip the root filesystem\n-T      Don't show title on startup\n-V      Verbose\n-C n    Write status information to specified file descriptor\n-t TYPE List of filesystem types to check"

#define HELP_fold "usage: fold [-bsu] [-w WIDTH] [FILE...]\n\nFolds (wraps) or unfolds ascii text by adding or removing newlines.\nDefault line width is 80 columns for folding and infinite for unfolding.\n\n-b	Fold based on bytes instead of columns\n-s	Fold/unfold at whitespace boundaries if possible\n-u	Unfold text (and refold if -w is given)\n-w	Set lines to WIDTH columns or bytes"

#define HELP_fdisk "usage: fdisk [-lu] [-C CYLINDERS] [-H HEADS] [-S SECTORS] [-b SECTSZ] DISK\n\nChange partition table\n\n-u            Start and End are in sectors (instead of cylinders)\n-l            Show partition table for each DISK, then exit\n-b size       sector size (512, 1024, 2048 or 4096)\n-C CYLINDERS  Set number of cylinders/heads/sectors\n-H HEADS\n-S SECTORS"

#define HELP_expr "usage: expr ARG1 OPERATOR ARG2...\n\nEvaluate expression and print result. For example, \"expr 1 + 2\".\n\nThe supported operators are (grouped from highest to lowest priority):\n\n  ( )    :    * / %    + -    != <= < >= > =    &    |\n\nEach constant and operator must be a separate command line argument.\nAll operators are infix, meaning they expect a constant (or expression\nthat resolves to a constant) on each side of the operator. Operators of\nthe same priority (within each group above) are evaluated left to right.\nParentheses may be used (as separate arguments) to elevate the priority\nof expressions.\n\nCalling expr from a command shell requires a lot of \\( or '*' escaping\nto avoid interpreting shell control characters.\n\nThe & and | operators are logical (not bitwise) and may operate on\nstrings (a blank string is \"false\"). Comparison operators may also\noperate on strings (alphabetical sort).\n\nConstants may be strings or integers. Comparison, logical, and regex\noperators may operate on strings (a blank string is \"false\"), other\noperators require integers."

#define HELP_dumpleases "usage: dumpleases [-r|-a] [-f LEASEFILE]\n\nDisplay DHCP leases granted by udhcpd\n-f FILE,  Lease file\n-r        Show remaining time\n-a        Show expiration time"

#define HELP_diff "usage: diff [-abBdiNqrTstw] [-L LABEL] [-S FILE] [-U LINES] FILE1 FILE2\n\n-a	Treat all files as text\n-b	Ignore changes in the amount of whitespace\n-B	Ignore changes whose lines are all blank\n-d	Try hard to find a smaller set of changes\n-i	Ignore case differences\n-L	Use LABEL instead of the filename in the unified header\n-N	Treat absent files as empty\n-q	Output only whether files differ\n-r	Recurse\n-S	Start with FILE when comparing directories\n-T	Make tabs line up by prefixing a tab when necessary\n-s	Report when two files are the same\n-t	Expand tabs to spaces in output\n-u	Unified diff\n-U	Output LINES lines of context\n-w	Ignore all whitespace\n\n--color              Colored output\n--strip-trailing-cr  Strip trailing '\\r's from input lines"

#define HELP_dhcpd "usage: dhcpd [-46fS] [-i IFACE] [-P N] [CONFFILE]\n\n -f    Run in foreground\n -i Interface to use\n -S    Log to syslog too\n -P N  Use port N (default ipv4 67, ipv6 547)\n -4, -6    Run as a DHCPv4 or DHCPv6 server"

#define HELP_dhcp6 "usage: dhcp6 [-fbnqvR] [-i IFACE] [-r IP] [-s PROG] [-p PIDFILE]\n\n      Configure network dynamically using DHCP.\n\n    -i Interface to use (default eth0)\n    -p Create pidfile\n    -s Run PROG at DHCP events\n    -t Send up to N Solicit packets\n    -T Pause between packets (default 3 seconds)\n    -A Wait N seconds after failure (default 20)\n    -f Run in foreground\n    -b Background if lease is not obtained\n    -n Exit if lease is not obtained\n    -q Exit after obtaining lease\n    -R Release IP on exit\n    -S Log to syslog too\n    -r Request this IP address\n    -v Verbose\n\n    Signals:\n    USR1  Renew current lease\n    USR2  Release current lease"

#define HELP_dhcp "usage: dhcp [-fbnqvoCRB] [-i IFACE] [-r IP] [-s PROG] [-p PIDFILE]\n            [-H HOSTNAME] [-V VENDOR] [-x OPT:VAL] [-O OPT]\n\n     Configure network dynamically using DHCP.\n\n   -i Interface to use (default eth0)\n   -p Create pidfile\n   -s Run PROG at DHCP events (default /usr/share/dhcp/default.script)\n   -B Request broadcast replies\n   -t Send up to N discover packets\n   -T Pause between packets (default 3 seconds)\n   -A Wait N seconds after failure (default 20)\n   -f Run in foreground\n   -b Background if lease is not obtained\n   -n Exit if lease is not obtained\n   -q Exit after obtaining lease\n   -R Release IP on exit\n   -S Log to syslog too\n   -a Use arping to validate offered address\n   -O Request option OPT from server (cumulative)\n   -o Don't request any options (unless -O is given)\n   -r Request this IP address\n   -x OPT:VAL  Include option OPT in sent packets (cumulative)\n   -F Ask server to update DNS mapping for NAME\n   -H Send NAME as client hostname (default none)\n   -V VENDOR Vendor identifier (default 'toybox VERSION')\n   -C Don't send MAC as client identifier\n   -v Verbose\n\n   Signals:\n   USR1  Renew current lease\n   USR2  Release current lease"

#define HELP_dd "usage: dd [if=FILE] [of=FILE] [ibs=N] [obs=N] [iflag=FLAGS] [oflag=FLAGS]\n        [bs=N] [count=N] [seek=N] [skip=N]\n        [conv=notrunc|noerror|sync|fsync] [status=noxfer|none]\n\nCopy/convert files.\n\nif=FILE		Read from FILE instead of stdin\nof=FILE		Write to FILE instead of stdout\nbs=N		Read and write N bytes at a time\nibs=N		Input block size\nobs=N		Output block size\ncount=N		Copy only N input blocks\nskip=N		Skip N input blocks\nseek=N		Skip N output blocks\niflag=FLAGS	Set input flags\noflag=FLAGS	Set output flags\nconv=notrunc	Don't truncate output file\nconv=noerror	Continue after read errors\nconv=sync	Pad blocks with zeros\nconv=fsync	Physically write data out before finishing\nstatus=noxfer	Don't show transfer rate\nstatus=none	Don't show transfer rate or records in/out\n\nFLAGS is a comma-separated list of:\n\ncount_bytes	(iflag) interpret count=N in bytes, not blocks\nseek_bytes	(oflag) interpret seek=N in bytes, not blocks\nskip_bytes	(iflag) interpret skip=N in bytes, not blocks\n\nNumbers may be suffixed by c (*1), w (*2), b (*512), kD (*1000), k (*1024),\nMD (*1000*1000), M (*1024*1024), GD (*1000*1000*1000) or G (*1024*1024*1024)."

#define HELP_crontab "usage: crontab [-u user] FILE\n               [-u user] [-e | -l | -r]\n               [-c dir]\n\nFiles used to schedule the execution of programs.\n\n-c crontab dir\n-e edit user's crontab\n-l list user's crontab\n-r delete user's crontab\n-u user\nFILE Replace crontab by FILE ('-': stdin)"

#define HELP_crond "usage: crond [-fbS] [-l N] [-d N] [-L LOGFILE] [-c DIR]\n\nA daemon to execute scheduled commands.\n\n-b Background (default)\n-c crontab dir\n-d Set log level, log to stderr\n-f Foreground\n-l Set log level. 0 is the most verbose, default 8\n-S Log to syslog (default)\n-L Log to file"

#define HELP_brctl "usage: brctl COMMAND [BRIDGE [INTERFACE]]\n\nManage ethernet bridges\n\nCommands:\nshow                  Show a list of bridges\naddbr BRIDGE          Create BRIDGE\ndelbr BRIDGE          Delete BRIDGE\naddif BRIDGE IFACE    Add IFACE to BRIDGE\ndelif BRIDGE IFACE    Delete IFACE from BRIDGE\nsetageing BRIDGE TIME Set ageing time\nsetfd BRIDGE TIME     Set bridge forward delay\nsethello BRIDGE TIME  Set hello time\nsetmaxage BRIDGE TIME Set max message age\nsetpathcost BRIDGE PORT COST   Set path cost\nsetportprio BRIDGE PORT PRIO   Set port priority\nsetbridgeprio BRIDGE PRIO      Set bridge priority\nstp BRIDGE [1/yes/on|0/no/off] STP on/off"

#define HELP_bootchartd "usage: bootchartd {start [PROG ARGS]}|stop|init\n\nCreate /var/log/bootlog.tgz with boot chart data\n\nstart: start background logging; with PROG, run PROG,\n       then kill logging with USR1\nstop:  send USR1 to all bootchartd processes\ninit:  start background logging; stop when getty/xdm is seen\n      (for init scripts)\n\nUnder PID 1: as init, then exec $bootchart_init, /init, /sbin/init"

#define HELP_bc "usage: bc [-ilqsw] [file ...]\n\nbc is a command-line calculator with a Turing-complete language.\n\noptions:\n\n  -i  --interactive  force interactive mode\n  -l  --mathlib      use predefined math routines:\n\n                     s(expr)  =  sine of expr in radians\n                     c(expr)  =  cosine of expr in radians\n                     a(expr)  =  arctangent of expr, returning radians\n                     l(expr)  =  natural log of expr\n                     e(expr)  =  raises e to the power of expr\n                     j(n, x)  =  Bessel function of integer order n of x\n\n  -q  --quiet        don't print version and copyright\n  -s  --standard     error if any non-POSIX extensions are used\n  -w  --warn         warn if any non-POSIX extensions are used"

#define HELP_arping "usage: arping [-fqbDUA] [-c CNT] [-w TIMEOUT] [-I IFACE] [-s SRC_IP] DST_IP\n\nSend ARP requests/replies\n\n-f         Quit on first ARP reply\n-q         Quiet\n-b         Keep broadcasting, don't go unicast\n-D         Duplicated address detection mode\n-U         Unsolicited ARP mode, update your neighbors\n-A         ARP answer mode, update your neighbors\n-c N       Stop after sending N ARP requests\n-w TIMEOUT Time to wait for ARP reply, seconds\n-I IFACE   Interface to use (default eth0)\n-s SRC_IP  Sender IP address\nDST_IP     Target IP address"

#define HELP_arp "usage: arp\n[-vn] [-H HWTYPE] [-i IF] -a [HOSTNAME]\n[-v]              [-i IF] -d HOSTNAME [pub]\n[-v]  [-H HWTYPE] [-i IF] -s HOSTNAME HWADDR [temp]\n[-v]  [-H HWTYPE] [-i IF] -s HOSTNAME HWADDR [netmask MASK] pub\n[-v]  [-H HWTYPE] [-i IF] -Ds HOSTNAME IFACE [netmask MASK] pub\n\nManipulate ARP cache\n\n-a    Display (all) hosts\n-s    Set new ARP entry\n-d    Delete a specified entry\n-v    Verbose\n-n    Don't resolve names\n-i IF Network interface\n-D    Read <hwaddr> from given device\n-A,-p AF  Protocol family\n-H    HWTYPE Hardware address type"

#define HELP_xargs "usage: xargs [-0prt] [-s NUM] [-n NUM] [-E STR] COMMAND...\n\nRun command line one or more times, appending arguments from stdin.\n\nIf COMMAND exits with 255, don't launch another even if arguments remain.\n\n-0	Each argument is NULL terminated, no whitespace or quote processing\n-E	Stop at line matching string\n-n	Max number of arguments per command\n-o	Open tty for COMMAND's stdin (default /dev/null)\n-p	Prompt for y/n from tty before running each command\n-r	Don't run command with empty input (otherwise always run command once)\n-s	Size in bytes per command line\n-t	Trace, print command line to stderr"

#define HELP_who "usage: who\n\nPrint information about logged in users."

#define HELP_wc "usage: wc -lwcm [FILE...]\n\nCount lines, words, and characters in input.\n\n-l	Show lines\n-w	Show words\n-c	Show bytes\n-m	Show characters\n\nBy default outputs lines, words, bytes, and filename for each\nargument (or from stdin if none). Displays only either bytes\nor characters."

#define HELP_uuencode "usage: uuencode [-m] [INFILE] ENCODE_FILENAME\n\nUuencode stdin (or INFILE) to stdout, with ENCODE_FILENAME in the output.\n\n-m	Base64"

#define HELP_uudecode "usage: uudecode [-o OUTFILE] [INFILE]\n\nDecode file from stdin (or INFILE).\n\n-o	Write to OUTFILE instead of filename in header"

#define HELP_unlink "usage: unlink FILE\n\nDelete one file."

#define HELP_uniq "usage: uniq [-cduiz] [-w MAXCHARS] [-f FIELDS] [-s CHAR] [INFILE [OUTFILE]]\n\nReport or filter out repeated lines in a file\n\n-c	Show counts before each line\n-d	Show only lines that are repeated\n-u	Show only lines that are unique\n-i	Ignore case when comparing lines\n-z	Lines end with \\0 not \\n\n-w	Compare maximum X chars per line\n-f	Ignore first X fields\n-s	Ignore first X chars"

#define HELP_uname "usage: uname [-asnrvm]\n\nPrint system information.\n\n-s	System name\n-n	Network (domain) name\n-r	Kernel Release number\n-v	Kernel Version\n-m	Machine (hardware) name\n-a	All of the above"

#define HELP_arch "usage: arch\n\nPrint machine (hardware) name, same as uname -m."

#define HELP_ulimit "usage: ulimit [-P PID] [-SHRacdefilmnpqrstuv] [LIMIT]\n\nPrint or set resource limits for process number PID. If no LIMIT specified\n(or read-only -ap selected) display current value (sizes in bytes).\nDefault is ulimit -P $PPID -Sf\" (show soft filesize of your shell).\n\n-S  Set/show soft limit          -H  Set/show hard (maximum) limit\n-a  Show all limits              -c  Core file size\n-d  Process data segment         -e  Max scheduling priority\n-f  Output file size             -i  Pending signal count\n-l  Locked memory                -m  Resident Set Size\n-n  Number of open files         -p  Pipe buffer\n-q  Posix message queue          -r  Max Real-time priority\n-R  Realtime latency (usec)      -s  Stack size\n-t  Total CPU time (in seconds)  -u  Maximum processes (under this UID)\n-v  Virtual memory size          -P  PID to affect (default $PPID)"

#define HELP_tty "usage: tty [-s]\n\nShow filename of terminal connected to stdin.\n\nPrints \"not a tty\" and exits with nonzero status if no terminal\nis connected to stdin.\n\n-s	Silent, exit code only"

#define HELP_true "usage: true\n\nReturn zero."

#define HELP_touch "usage: touch [-amch] [-d DATE] [-t TIME] [-r FILE] FILE...\n\nUpdate the access and modification times of each FILE to the current time.\n\n-a	Change access time\n-m	Change modification time\n-c	Don't create file\n-h	Change symlink\n-d	Set time to DATE (in YYYY-MM-DDThh:mm:SS[.frac][tz] format)\n-t	Set time to TIME (in [[CC]YY]MMDDhhmm[.ss][frac] format)\n-r	Set time same as reference FILE"

#define HELP_time "usage: time [-pv] COMMAND...\n\nRun command line and report real, user, and system time elapsed in seconds.\n(real = clock on the wall, user = cpu used by command's code,\nsystem = cpu used by OS on behalf of command.)\n\n-p	POSIX format output (default)\n-v	Verbose"

#define HELP_test "usage: test [-bcdefghLPrSsuwx PATH] [-nz STRING] [-t FD] [X ?? Y]\n\nReturn true or false by performing tests. (With no arguments return false.)\n\n--- Tests with a single argument (after the option):\nPATH is/has:\n  -b  block device   -f  regular file   -p  fifo           -u  setuid bit\n  -c  char device    -g  setgid         -r  read bit       -w  write bit\n  -d  directory      -h  symlink        -S  socket         -x  execute bit\n  -e  exists         -L  symlink        -s  nonzero size\nSTRING is:\n  -n  nonzero size   -z  zero size      (STRING by itself implies -n)\nFD (integer file descriptor) is:\n  -t  a TTY\n\n--- Tests with one argument on each side of an operator:\nTwo strings:\n  =  are identical   !=  differ\nTwo integers:\n  -eq  equal         -gt  first > second    -lt  first < second\n  -ne  not equal     -ge  first >= second   -le  first <= second\n\n--- Modify or combine tests:\n  ! EXPR     not (swap true/false)   EXPR -a EXPR    and (are both true)\n  ( EXPR )   evaluate this first     EXPR -o EXPR    or (is either true)"

#define HELP_tee "usage: tee [-ai] [FILE...]\n\nCopy stdin to each listed file, and also to stdout.\nFilename \"-\" is a synonym for stdout.\n\n-a	Append to files\n-i	Ignore SIGINT"

#define HELP_tar "usage: tar [-cxt] [-fvohmjkOS] [-XTCf NAME] [FILE...]\n\nCreate, extract, or list files in a .tar (or compressed t?z) file.\n\nOptions:\nc  Create                x  Extract               t  Test (list)\nf  tar FILE (default -)  C  Change to DIR first   v  Verbose display\no  Ignore owner          h  Follow symlinks       m  Ignore mtime\nJ  xz compression        j  bzip2 compression     z  gzip compression\nO  Extract to stdout     X  exclude names in FILE T  include names in FILE\n\n--exclude        FILENAME to exclude    --full-time   Show seconds with -tv\n--mode MODE      Adjust modes           --mtime TIME  Override timestamps\n--owner NAME     Set file owner to NAME --group NAME  Set file group to NAME\n--sparse         Record sparse files\n--restrict       All archive contents must extract under one subdirctory\n--numeric-owner  Save/use/display uid and gid, not user/group name\n--no-recursion   Don't store directory contents"

#define HELP_tail "usage: tail [-n|c NUMBER] [-f] [FILE...]\n\nCopy last lines from files to stdout. If no files listed, copy from\nstdin. Filename \"-\" is a synonym for stdin.\n\n-n	Output the last NUMBER lines (default 10), +X counts from start\n-c	Output the last NUMBER bytes, +NUMBER counts from start\n-f	Follow FILE(s), waiting for more data to be appended"

#define HELP_strings "usage: strings [-fo] [-t oxd] [-n LEN] [FILE...]\n\nDisplay printable strings in a binary file\n\n-f	Show filename\n-n	At least LEN characters form a string (default 4)\n-o	Show offset (ala -t d)\n-t	Show offset type (o=octal, d=decimal, x=hexadecimal)"

#define HELP_split "usage: split [-a SUFFIX_LEN] [-b BYTES] [-l LINES] [INPUT [OUTPUT]]\n\nCopy INPUT (or stdin) data to a series of OUTPUT (or \"x\") files with\nalphabetically increasing suffix (aa, ab, ac... az, ba, bb...).\n\n-a	Suffix length (default 2)\n-b	BYTES/file (10, 10k, 10m, 10g...)\n-l	LINES/file (default 1000)"

#define HELP_sort "usage: sort [-Mbcdfginrsuz] [FILE...] [-k#[,#[x]] [-t X]] [-o FILE]\n\nSort all lines of text from input files (or stdin) to stdout.\n-M	Month sort (jan, feb, etc)\n-V	Version numbers (name-1.234-rc6.5b.tgz)\n-b	Ignore leading blanks (or trailing blanks in second part of key)\n-c	Check whether input is sorted\n-d	Dictionary order (use alphanumeric and whitespace chars only)\n-f	Force uppercase (case insensitive sort)\n-g	General numeric sort (double precision with nan and inf)\n-i	Ignore nonprinting characters\n-k	Sort by \"key\" (see below)\n-n	Numeric order (instead of alphabetical)\n-o	Output to FILE instead of stdout\n-r	Reverse\n-s	Skip fallback sort (only sort with keys)\n-t	Use a key separator other than whitespace\n-u	Unique lines only\n-x	Hexadecimal numerical sort\n-z	Zero (null) terminated lines\n\nSorting by key looks at a subset of the words on each line. -k2 uses the\nsecond word to the end of the line, -k2,2 looks at only the second word,\n-k2,4 looks from the start of the second to the end of the fourth word.\n-k2.4,5 starts from the fourth character of the second word, to the end\nof the fifth word. Specifying multiple keys uses the later keys as tie\nbreakers, in order. A type specifier appended to a sort key (such as -2,2n)\napplies only to sorting that key."

#define HELP_sleep "usage: sleep DURATION\n\nWait before exiting.\n\nDURATION can be a decimal fraction. An optional suffix can be \"m\"\n(minutes), \"h\" (hours), \"d\" (days), or \"s\" (seconds, the default)."

#define HELP_sed "usage: sed [-inrzE] [-e SCRIPT]...|SCRIPT [-f SCRIPT_FILE]... [FILE...]\n\nStream editor. Apply one or more editing SCRIPTs to each line of input\n(from FILE or stdin) producing output (by default to stdout).\n\n-e	Add SCRIPT to list\n-f	Add contents of SCRIPT_FILE to list\n-i	Edit each file in place (-iEXT keeps backup file with extension EXT)\n-n	No default output (use the p command to output matched lines)\n-r	Use extended regular expression syntax\n-E	POSIX alias for -r\n-s	Treat input files separately (implied by -i)\n-z	Use \\0 rather than \\n as the input line separator\n\nA SCRIPT is a series of one or more COMMANDs separated by newlines or\nsemicolons. All -e SCRIPTs are concatenated together as if separated\nby newlines, followed by all lines from -f SCRIPT_FILEs, in order.\nIf no -e or -f SCRIPTs are specified, the first argument is the SCRIPT.\n\nEach COMMAND may be preceded by an address which limits the command to\napply only to the specified line(s). Commands without an address apply to\nevery line. Addresses are of the form:\n\n  [ADDRESS[,ADDRESS]][!]COMMAND\n\nThe ADDRESS may be a decimal line number (starting at 1), a /regular\nexpression/ within a pair of forward slashes, or the character \"$\" which\nmatches the last line of input. (In -s or -i mode this matches the last\nline of each file, otherwise just the last line of the last file.) A single\naddress matches one line, a pair of comma separated addresses match\neverything from the first address to the second address (inclusive). If\nboth addresses are regular expressions, more than one range of lines in\neach file can match. The second address can be +N to end N lines later.\n\nREGULAR EXPRESSIONS in sed are started and ended by the same character\n(traditionally / but anything except a backslash or a newline works).\nBackslashes may be used to escape the delimiter if it occurs in the\nregex, and for the usual printf escapes (\\abcefnrtv and octal, hex,\nand unicode). An empty regex repeats the previous one. ADDRESS regexes\n(above) require the first delimiter to be escaped with a backslash when\nit isn't a forward slash (to distinguish it from the COMMANDs below).\n\nSed mostly operates on individual lines one at a time. It reads each line,\nprocesses it, and either writes it to the output or discards it before\nreading the next line. Sed can remember one additional line in a separate\nbuffer (using the h, H, g, G, and x commands), and can read the next line\nof input early (using the n and N command), but other than that command\nscripts operate on individual lines of text.\n\nEach COMMAND starts with a single character. The following commands take\nno arguments:\n\n  !  Run this command when the test _didn't_ match.\n\n  {  Start a new command block, continuing until a corresponding \"}\".\n     Command blocks may nest. If the block has an address, commands within\n     the block are only run for lines within the block's address range.\n\n  }  End command block (this command cannot have an address)\n\n  d  Delete this line and move on to the next one\n     (ignores remaining COMMANDs)\n\n  D  Delete one line of input and restart command SCRIPT (same as \"d\"\n     unless you've glued lines together with \"N\" or similar)\n\n  g  Get remembered line (overwriting current line)\n\n  G  Get remembered line (appending to current line)\n\n  h  Remember this line (overwriting remembered line)\n\n  H  Remember this line (appending to remembered line, if any)\n\n  l  Print line, escaping \\abfrtv (but not newline), octal escaping other\n     nonprintable characters, wrapping lines to terminal width with a\n     backslash, and appending $ to actual end of line.\n\n  n  Print default output and read next line, replacing current line\n     (If no next line available, quit processing script)\n\n  N  Append next line of input to this line, separated by a newline\n     (This advances the line counter for address matching and \"=\", if no\n     next line available quit processing script without default output)\n\n  p  Print this line\n\n  P  Print this line up to first newline (from \"N\")\n\n  q  Quit (print default output, no more commands processed or lines read)\n\n  x  Exchange this line with remembered line (overwrite in both directions)\n\n  =  Print the current line number (followed by a newline)\n\nThe following commands (may) take an argument. The \"text\" arguments (to\nthe \"a\", \"b\", and \"c\" commands) may end with an unescaped \"\\\" to append\nthe next line (for which leading whitespace is not skipped), and also\ntreat \";\" as a literal character (use \"\\;\" instead).\n\n  a [text]   Append text to output before attempting to read next line\n\n  b [label]  Branch, jumps to :label (or with no label, to end of SCRIPT)\n\n  c [text]   Delete line, output text at end of matching address range\n             (ignores remaining COMMANDs)\n\n  i [text]   Print text\n\n  r [file]   Append contents of file to output before attempting to read\n             next line.\n\n  s/S/R/F    Search for regex S, replace matched text with R using flags F.\n             The first character after the \"s\" (anything but newline or\n             backslash) is the delimiter, escape with \\ to use normally.\n\n             The replacement text may contain \"&\" to substitute the matched\n             text (escape it with backslash for a literal &), or \\1 through\n             \\9 to substitute a parenthetical subexpression in the regex.\n             You can also use the normal backslash escapes such as \\n and\n             a backslash at the end of the line appends the next line.\n\n             The flags are:\n\n             [0-9]    A number, substitute only that occurrence of pattern\n             g        Global, substitute all occurrences of pattern\n             i        Ignore case when matching\n             p        Print the line if match was found and replaced\n             w [file] Write (append) line to file if match replaced\n\n  t [label]  Test, jump to :label only if an \"s\" command found a match in\n             this line since last test (replacing with same text counts)\n\n  T [label]  Test false, jump only if \"s\" hasn't found a match.\n\n  w [file]   Write (append) line to file\n\n  y/old/new/ Change each character in 'old' to corresponding character\n             in 'new' (with standard backslash escapes, delimiter can be\n             any repeated character except \\ or \\n)\n\n  : [label]  Labeled target for jump commands\n\n  #  Comment, ignore rest of this line of SCRIPT\n\nDeviations from POSIX: allow extended regular expressions with -r,\nediting in place with -i, separate with -s, NUL-separated input with -z,\nprintf escapes in text, line continuations, semicolons after all commands,\n2-address anywhere an address is allowed, \"T\" command, multiline\ncontinuations for [abc], \\; to end [abc] argument before end of line."

#define HELP_rmdir "usage: rmdir [-p] [DIR...]\n\nRemove one or more directories.\n\n-p	Remove path\n--ignore-fail-on-non-empty	Ignore failures caused by non-empty directories"

#define HELP_rm "usage: rm [-fiRrv] FILE...\n\nRemove each argument from the filesystem.\n\n-f	Force: remove without confirmation, no error if it doesn't exist\n-i	Interactive: prompt for confirmation\n-rR	Recursive: remove directory contents\n-v	Verbose"

#define HELP_renice "usage: renice [-gpu] -n INCREMENT ID..."

#define HELP_pwd "usage: pwd [-L|-P]\n\nPrint working (current) directory.\n\n-L	Use shell's path from $PWD (when applicable)\n-P	Print canonical absolute path"

#define HELP_pkill "usage: pkill [-fnovx] [-SIGNAL|-l SIGNAL] [PATTERN] [-G GID,] [-g PGRP,] [-P PPID,] [-s SID,] [-t TERM,] [-U UID,] [-u EUID,]\n\n-l	Send SIGNAL (default SIGTERM)\n-V	Verbose\n-f	Check full command line for PATTERN\n-G	Match real Group ID(s)\n-g	Match Process Group(s) (0 is current user)\n-n	Newest match only\n-o	Oldest match only\n-P	Match Parent Process ID(s)\n-s	Match Session ID(s) (0 for current)\n-t	Match Terminal(s)\n-U	Match real User ID(s)\n-u	Match effective User ID(s)\n-v	Negate the match\n-x	Match whole command (not substring)"

#define HELP_pgrep "usage: pgrep [-clfnovx] [-d DELIM] [-L SIGNAL] [PATTERN] [-G GID,] [-g PGRP,] [-P PPID,] [-s SID,] [-t TERM,] [-U UID,] [-u EUID,]\n\nSearch for process(es). PATTERN is an extended regular expression checked\nagainst command names.\n\n-c	Show only count of matches\n-d	Use DELIM instead of newline\n-L	Send SIGNAL instead of printing name\n-l	Show command name\n-f	Check full command line for PATTERN\n-G	Match real Group ID(s)\n-g	Match Process Group(s) (0 is current user)\n-n	Newest match only\n-o	Oldest match only\n-P	Match Parent Process ID(s)\n-s	Match Session ID(s) (0 for current)\n-t	Match Terminal(s)\n-U	Match real User ID(s)\n-u	Match effective User ID(s)\n-v	Negate the match\n-x	Match whole command (not substring)"

#define HELP_iotop "usage: iotop [-AaKObq] [-n NUMBER] [-d SECONDS] [-p PID,] [-u USER,]\n\nRank processes by I/O.\n\n-A	All I/O, not just disk\n-a	Accumulated I/O (not percentage)\n-H	Show threads\n-K	Kilobytes\n-k	Fallback sort FIELDS (default -[D]IO,-ETIME,-PID)\n-m	Maximum number of tasks to show\n-O	Only show processes doing I/O\n-o	Show FIELDS (default PID,PR,USER,[D]READ,[D]WRITE,SWAP,[D]IO,COMM)\n-s	Sort by field number (0-X, default 6)\n-b	Batch mode (no tty)\n-d	Delay SECONDS between each cycle (default 3)\n-n	Exit after NUMBER iterations\n-p	Show these PIDs\n-u	Show these USERs\n-q	Quiet (no header lines)\n\nCursor LEFT/RIGHT to change sort, UP/DOWN move list, space to force\nupdate, R to reverse sort, Q to exit."

#define HELP_top "usage: top [-Hbq] [-k FIELD,] [-o FIELD,] [-s SORT] [-n NUMBER] [-m LINES] [-d SECONDS] [-p PID,] [-u USER,]\n\nShow process activity in real time.\n\n-H	Show threads\n-k	Fallback sort FIELDS (default -S,-%CPU,-ETIME,-PID)\n-o	Show FIELDS (def PID,USER,PR,NI,VIRT,RES,SHR,S,%CPU,%MEM,TIME+,CMDLINE)\n-O	Add FIELDS (replacing PR,NI,VIRT,RES,SHR,S from default)\n-s	Sort by field number (1-X, default 9)\n-b	Batch mode (no tty)\n-d	Delay SECONDS between each cycle (default 3)\n-m	Maximum number of tasks to show\n-n	Exit after NUMBER iterations\n-p	Show these PIDs\n-u	Show these USERs\n-q	Quiet (no header lines)\n\nCursor LEFT/RIGHT to change sort, UP/DOWN move list, space to force\nupdate, R to reverse sort, Q to exit."

#define HELP_ps "usage: ps [-AadefLlnwZ] [-gG GROUP,] [-k FIELD,] [-o FIELD,] [-p PID,] [-t TTY,] [-uU USER,]\n\nList processes.\n\nWhich processes to show (-gGuUpPt selections may be comma separated lists):\n\n-A  All					-a  Has terminal not session leader\n-d  All but session leaders		-e  Synonym for -A\n-g  In GROUPs				-G  In real GROUPs (before sgid)\n-p  PIDs (--pid)			-P  Parent PIDs (--ppid)\n-s  In session IDs			-t  Attached to selected TTYs\n-T  Show threads also			-u  Owned by selected USERs\n-U  Real USERs (before suid)\n\nOutput modifiers:\n\n-k  Sort FIELDs (-FIELD to reverse)	-M  Measure/pad future field widths\n-n  Show numeric USER and GROUP		-w  Wide output (don't truncate fields)\n\nWhich FIELDs to show. (-o HELP for list, default = -o PID,TTY,TIME,CMD)\n\n-f  Full listing (-o USER:12=UID,PID,PPID,C,STIME,TTY,TIME,ARGS=CMD)\n-l  Long listing (-o F,S,UID,PID,PPID,C,PRI,NI,ADDR,SZ,WCHAN,TTY,TIME,CMD)\n-o  Output FIELDs instead of defaults, each with optional :size and =title\n-O  Add FIELDS to defaults\n-Z  Include LABEL"

#define HELP_printf "usage: printf FORMAT [ARGUMENT...]\n\nFormat and print ARGUMENT(s) according to FORMAT, using C printf syntax\n(% escapes for cdeEfgGiosuxX, \\ escapes for abefnrtv0 or \\OCTAL or \\xHEX)."

#define HELP_patch "usage: patch [-d DIR] [-i PATCH] [-p DEPTH] [-F FUZZ] [-Rlsu] [--dry-run] [FILE [PATCH]]\n\nApply a unified diff to one or more files.\n\n-d	Modify files in DIR\n-i	Input patch file (default=stdin)\n-l	Loose match (ignore whitespace)\n-p	Number of '/' to strip from start of file paths (default=all)\n-R	Reverse patch\n-s	Silent except for errors\n-u	Ignored (only handles \"unified\" diffs)\n--dry-run Don't change files, just confirm patch applies\n\nThis version of patch only handles unified diffs, and only modifies\na file when all hunks to that file apply. Patch prints failed hunks\nto stderr, and exits with nonzero status if any hunks fail.\n\nA file compared against /dev/null (or with a date <= the epoch) is\ncreated/deleted as appropriate."

#define HELP_paste "usage: paste [-s] [-d DELIMITERS] [FILE...]\n\nMerge corresponding lines from each input file.\n\n-d	List of delimiter characters to separate fields with (default is \\t)\n-s	Sequential mode: turn each input file into one line of output"

#define HELP_od "usage: od [-bcdosxv] [-j #] [-N #] [-w #] [-A doxn] [-t acdfoux[#]]\n\nDump data in octal/hex.\n\n-A	Address base (decimal, octal, hexadecimal, none)\n-j	Skip this many bytes of input\n-N	Stop dumping after this many bytes\n-t	Output type a(scii) c(har) d(ecimal) f(loat) o(ctal) u(nsigned) (he)x\n	plus optional size in bytes\n	aliases: -b=-t o1, -c=-t c, -d=-t u2, -o=-t o2, -s=-t d2, -x=-t x2\n-v	Don't collapse repeated lines together\n-w	Total line width in bytes (default 16)"

#define HELP_nohup "usage: nohup COMMAND...\n\nRun a command that survives the end of its terminal.\n\nRedirect tty on stdin to /dev/null, tty on stdout to \"nohup.out\"."

#define HELP_nl "usage: nl [-E] [-l #] [-b MODE] [-n STYLE] [-s SEPARATOR] [-v #] [-w WIDTH] [FILE...]\n\nNumber lines of input.\n\n-E	Use extended regex syntax (when doing -b pREGEX)\n-b	Which lines to number: a (all) t (non-empty, default) pREGEX (pattern)\n-l	Only count last of this many consecutive blank lines\n-n	Number STYLE: ln (left justified) rn (right justified) rz (zero pad)\n-s	Separator to use between number and line (instead of TAB)\n-v	Starting line number for each section (default 1)\n-w	Width of line numbers (default 6)"

#define HELP_nice "usage: nice [-n PRIORITY] COMMAND...\n\nRun a command line at an increased or decreased scheduling priority.\n\nHigher numbers make a program yield more CPU time, from -20 (highest\npriority) to 19 (lowest).  By default processes inherit their parent's\nniceness (usually 0).  By default this command adds 10 to the parent's\npriority.  Only root can set a negative niceness level."

#define HELP_mkfifo "usage: mkfifo [-Z CONTEXT] [NAME...]\n\nCreate FIFOs (named pipes).\n\n-Z	Security context"

#define HELP_mkdir_z "usage: [-Z context]\n\n-Z	Set security context"

#define HELP_mkdir "usage: mkdir [-vp] [-m MODE] [DIR...]\n\nCreate one or more directories.\n\n-m	Set permissions of directory to mode\n-p	Make parent directories as needed\n-v	Verbose"

#define HELP_ls "usage: ls [-ACFHLRSZacdfhiklmnpqrstuwx1] [--color[=auto]] [FILE...]\n\nList files.\n\nwhat to show:\n-a  all files including .hidden    -b  escape nongraphic chars\n-c  use ctime for timestamps       -d  directory, not contents\n-i  inode number                   -p  put a '/' after dir names\n-q  unprintable chars as '?'       -s  storage used (1024 byte units)\n-u  use access time for timestamps -A  list all files but . and ..\n-H  follow command line symlinks   -L  follow symlinks\n-R  recursively list in subdirs    -F  append /dir *exe @sym |FIFO\n-Z  security context\n\noutput formats:\n-1  list one file per line         -C  columns (sorted vertically)\n-g  like -l but no owner           -h  human readable sizes\n-l  long (show full details)       -m  comma separated\n-n  like -l but numeric uid/gid    -o  like -l but no group\n-w  set column width               -x  columns (horizontal sort)\n-ll long with nanoseconds (--full-time)\n--color  device=yellow  symlink=turquoise/red  dir=blue  socket=purple\n         files: exe=green  suid=red  suidfile=redback  stickydir=greenback\n         =auto means detect if output is a tty.\n\nsorting (default is alphabetical):\n-f  unsorted    -r  reverse    -t  timestamp    -S  size"

#define HELP_logger "usage: logger [-s] [-t TAG] [-p [FACILITY.]PRIORITY] [MESSAGE...]\n\nLog message (or stdin) to syslog.\n\n-s	Also write message to stderr\n-t	Use TAG instead of username to identify message source\n-p	Specify PRIORITY with optional FACILITY. Default is \"user.notice\""

#define HELP_ln "usage: ln [-sfnv] [-t DIR] [FROM...] TO\n\nCreate a link between FROM and TO.\nOne/two/many arguments work like \"mv\" or \"cp\".\n\n-s	Create a symbolic link\n-f	Force the creation of the link, even if TO already exists\n-n	Symlink at TO treated as file\n-r	Create relative symlink from -> to\n-t	Create links in DIR\n-T	TO always treated as file, max 2 arguments\n-v	Verbose"

#define HELP_link "usage: link FILE NEWLINK\n\nCreate hardlink to a file."

#define HELP_killall5 "usage: killall5 [-l [SIGNAL]] [-SIGNAL|-s SIGNAL] [-o PID]...\n\nSend a signal to all processes outside current session.\n\n-l	List signal name(s) and number(s)\n-o PID	Omit PID\n-s	Send SIGNAL (default SIGTERM)"

#define HELP_kill "usage: kill [-l [SIGNAL] | -s SIGNAL | -SIGNAL] PID...\n\nSend signal to process(es).\n\n-l	List signal name(s) and number(s)\n-s	Send SIGNAL (default SIGTERM)"

#define HELP_whoami "usage: whoami\n\nPrint the current user name."

#define HELP_logname "usage: logname\n\nPrint the current user name."

#define HELP_groups "usage: groups [user]\n\nPrint the groups a user is in."

#define HELP_id "usage: id [-GZgnru] [USER...]\n\nPrint user and group ID.\n-G	Show all group IDs\n-Z	Show only security context\n-g	Show only the effective group ID\n-n	Print names instead of numeric IDs (to be used with -Ggu)\n-r	Show real ID instead of effective ID\n-u	Show only the effective user ID"

#define HELP_iconv "usage: iconv [-f FROM] [-t TO] [FILE...]\n\nConvert character encoding of files.\n\n-c	Omit invalid chars\n-f	Convert from (default UTF-8)\n-t	Convert to   (default UTF-8)"

#define HELP_head "usage: head [-n NUM] [FILE...]\n\nCopy first lines from files to stdout. If no files listed, copy from\nstdin. Filename \"-\" is a synonym for stdin.\n\n-n	Number of lines to copy\n-c	Number of bytes to copy\n-q	Never print headers\n-v	Always print headers"

#define HELP_grep "usage: grep [-EFrivwcloqsHbhn] [-ABC NUM] [-m MAX] [-e REGEX]... [-MS PATTERN]... [-f REGFILE] [FILE]...\n\nShow lines matching regular expressions. If no -e, first argument is\nregular expression to match. With no files (or \"-\" filename) read stdin.\nReturns 0 if matched, 1 if no match found, 2 for command errors.\n\n-e  Regex to match. (May be repeated.)\n-f  File listing regular expressions to match.\n\nfile search:\n-r  Recurse into subdirectories (defaults FILE to \".\")\n-R  Recurse into subdirectories and symlinks to directories\n-M  Match filename pattern (--include)\n-S  Skip filename pattern (--exclude)\n--exclude-dir=PATTERN  Skip directory pattern\n-I  Ignore binary files\n\nmatch type:\n-A  Show NUM lines after     -B  Show NUM lines before match\n-C  NUM lines context (A+B)  -E  extended regex syntax\n-F  fixed (literal match)    -a  always text (not binary)\n-i  case insensitive         -m  match MAX many lines\n-v  invert match             -w  whole word (implies -E)\n-x  whole line               -z  input NUL terminated\n\ndisplay modes: (default: matched line)\n-c  count of matching lines  -l  show only matching filenames\n-o  only matching part       -q  quiet (errors only)\n-s  silent (no error msg)    -Z  output NUL terminated\n\noutput prefix (default: filename if checking more than 1 file)\n-H  force filename           -b  byte offset of match\n-h  hide filename            -n  line number of match"

#define HELP_getconf "usage: getconf -a [PATH] | -l | NAME [PATH]\n\nGet system configuration values. Values from pathconf(3) require a path.\n\n-a	Show all (defaults to \"/\" if no path given)\n-l	List available value names (grouped by source)"

#define HELP_find "usage: find [-HL] [DIR...] [<options>]\n\nSearch directories for matching files.\nDefault: search \".\", match all, -print matches.\n\n-H  Follow command line symlinks         -L  Follow all symlinks\n\nMatch filters:\n-name  PATTERN   filename with wildcards  (-iname case insensitive)\n-path  PATTERN   path name with wildcards (-ipath case insensitive)\n-user  UNAME     belongs to user UNAME     -nouser     user ID not known\n-group GROUP     belongs to group GROUP    -nogroup    group ID not known\n-perm  [-/]MODE  permissions (-=min /=any) -prune      ignore dir contents\n-size  N[c]      512 byte blocks (c=bytes) -xdev       only this filesystem\n-links N         hardlink count            -atime N[u] accessed N units ago\n-ctime N[u]      created N units ago       -mtime N[u] modified N units ago\n-newer FILE      newer mtime than FILE     -mindepth N at least N dirs down\n-depth           ignore contents of dir    -maxdepth N at most N dirs down\n-inum N          inode number N            -empty      empty files and dirs\n-type [bcdflps]  type is (block, char, dir, file, symlink, pipe, socket)\n-true            always true               -false      always false\n-context PATTERN security context\n-newerXY FILE    X=acm time > FILE's Y=acm time (Y=t: FILE is literal time)\n\nNumbers N may be prefixed by a - (less than) or + (greater than). Units for\n-Xtime are d (days, default), h (hours), m (minutes), or s (seconds).\n\nCombine matches with:\n!, -a, -o, ( )    not, and, or, group expressions\n\nActions:\n-print  Print match with newline  -print0        Print match with null\n-exec   Run command with path     -execdir       Run command in file's dir\n-ok     Ask before exec           -okdir         Ask before execdir\n-delete Remove matching file/dir  -printf FORMAT Print using format string\n\nCommands substitute \"{}\" with matched file. End with \";\" to run each file,\nor \"+\" (next argument after \"{}\") to collect and run with multiple files.\n\n-printf FORMAT characters are \\ escapes and:\n%b  512 byte blocks used\n%f  basename            %g  textual gid          %G  numeric gid\n%i  decimal inode       %l  target of symlink    %m  octal mode\n%M  ls format type/mode %p  path to file         %P  path to file minus DIR\n%s  size in bytes       %T@ mod time as unixtime\n%u  username            %U  numeric uid          %Z  security context"

#define HELP_file "usage: file [-bhLs] [FILE...]\n\nExamine the given files and describe their content types.\n\n-b	Brief (no filename)\n-h	Don't follow symlinks (default)\n-L	Follow symlinks\n-s	Show block/char device contents"

#define HELP_false "usage: false\n\nReturn nonzero."

#define HELP_expand "usage: expand [-t TABLIST] [FILE...]\n\nExpand tabs to spaces according to tabstops.\n\n-t	TABLIST\n\nSpecify tab stops, either a single number instead of the default 8,\nor a comma separated list of increasing numbers representing tabstop\npositions (absolute, not increments) with each additional tab beyond\nthat becoming one space."

#define HELP_env "usage: env [-i] [-u NAME] [NAME=VALUE...] [COMMAND...]\n\nSet the environment for command invocation, or list environment variables.\n\n-i	Clear existing environment\n-u NAME	Remove NAME from the environment\n-0	Use null instead of newline in output"

#define HELP_echo "usage: echo [-neE] [ARG...]\n\nWrite each argument to stdout, with one space between each, followed\nby a newline.\n\n-n	No trailing newline\n-E	Print escape sequences literally (default)\n-e	Process the following escape sequences:\n	\\\\	Backslash\n	\\0NNN	Octal values (1 to 3 digits)\n	\\a	Alert (beep/flash)\n	\\b	Backspace\n	\\c	Stop output here (avoids trailing newline)\n	\\f	Form feed\n	\\n	Newline\n	\\r	Carriage return\n	\\t	Horizontal tab\n	\\v	Vertical tab\n	\\xHH	Hexadecimal values (1 to 2 digits)"

#define HELP_du "usage: du [-d N] [-askxHLlmc] [FILE...]\n\nShow disk usage, space consumed by files and directories.\n\nSize in:\n-k	1024 byte blocks (default)\n-K	512 byte blocks (posix)\n-m	Megabytes\n-h	Human readable (e.g., 1K 243M 2G)\n\nWhat to show:\n-a	All files, not just directories\n-H	Follow symlinks on cmdline\n-L	Follow all symlinks\n-s	Only total size of each argument\n-x	Don't leave this filesystem\n-c	Cumulative total\n-d N	Only depth < N\n-l	Disable hardlink filter"

#define HELP_dirname "usage: dirname PATH...\n\nShow directory portion of path."

#define HELP_df "usage: df [-HPkhi] [-t type] [FILE...]\n\nThe \"disk free\" command shows total/used/available disk space for\neach filesystem listed on the command line, or all currently mounted\nfilesystems.\n\n-a	Show all (including /proc and friends)\n-P	The SUSv3 \"Pedantic\" option\n-k	Sets units back to 1024 bytes (the default without -P)\n-h	Human readable (K=1024)\n-H	Human readable (k=1000)\n-i	Show inodes instead of blocks\n-t type	Display only filesystems of this type\n\nPedantic provides a slightly less useful output format dictated by Posix,\nand sets the units to 512 bytes instead of the default 1024 bytes."

#define HELP_date "usage: date [-u] [-r FILE] [-d DATE] [+DISPLAY_FORMAT] [-D SET_FORMAT] [SET]\n\nSet/get the current date/time. With no SET shows the current date.\n\n-d	Show DATE instead of current time (convert date format)\n-D	+FORMAT for SET or -d (instead of MMDDhhmm[[CC]YY][.ss])\n-r	Use modification time of FILE instead of current date\n-u	Use UTC instead of current timezone\n\nSupported input formats:\n\nMMDDhhmm[[CC]YY][.ss]     POSIX\n@UNIXTIME[.FRACTION]      seconds since midnight 1970-01-01\nYYYY-MM-DD [hh:mm[:ss]]   ISO 8601\nhh:mm[:ss]                24-hour time today\n\nAll input formats can be preceded by TZ=\"id\" to set the input time zone\nseparately from the output time zone. Otherwise $TZ sets both.\n\n+FORMAT specifies display format string using strftime(3) syntax:\n\n%% literal %             %n newline              %t tab\n%S seconds (00-60)       %M minute (00-59)       %m month (01-12)\n%H hour (0-23)           %I hour (01-12)         %p AM/PM\n%y short year (00-99)    %Y year                 %C century\n%a short weekday name    %A weekday name         %u day of week (1-7, 1=mon)\n%b short month name      %B month name           %Z timezone name\n%j day of year (001-366) %d day of month (01-31) %e day of month ( 1-31)\n%N nanosec (output only)\n\n%U Week of year (0-53 start sunday)   %W Week of year (0-53 start monday)\n%V Week of year (1-53 start monday, week < 4 days not part of this year)\n\n%F \"%Y-%m-%d\"     %R \"%H:%M\"        %T \"%H:%M:%S\"    %z numeric timezone\n%D \"%m/%d/%y\"     %r \"%I:%M:%S %p\"  %h \"%b\"          %s unix epoch time\n%x locale date    %X locale time    %c locale date/time"

#define HELP_cut "usage: cut [-Ds] [-bcfF LIST] [-dO DELIM] [FILE...]\n\nPrint selected parts of lines from each FILE to standard output.\n\nEach selection LIST is comma separated, either numbers (counting from 1)\nor dash separated ranges (inclusive, with X- meaning to end of line and -X\nfrom start). By default selection ranges are sorted and collated, use -D\nto prevent that.\n\n-b	Select bytes\n-c	Select UTF-8 characters\n-C	Select unicode columns\n-d	Use DELIM (default is TAB for -f, run of whitespace for -F)\n-D	Don't sort/collate selections or match -fF lines without delimiter\n-f	Select fields (words) separated by single DELIM character\n-F	Select fields separated by DELIM regex\n-O	Output delimiter (default one space for -F, input delim for -f)\n-s	Skip lines without delimiters"

#define HELP_cpio "usage: cpio -{o|t|i|p DEST} [-v] [--verbose] [-F FILE] [--no-preserve-owner]\n       [ignored: -mdu -H newc]\n\nCopy files into and out of a \"newc\" format cpio archive.\n\n-F FILE	Use archive FILE instead of stdin/stdout\n-p DEST	Copy-pass mode, copy stdin file list to directory DEST\n-i	Extract from archive into file system (stdin=archive)\n-o	Create archive (stdin=list of files, stdout=archive)\n-t	Test files (list only, stdin=archive, stdout=list of files)\n-v	Verbose\n--no-preserve-owner (don't set ownership during extract)\n--trailer Add legacy trailer (prevents concatenation)"

#define HELP_install "usage: install [-dDpsv] [-o USER] [-g GROUP] [-m MODE] [SOURCE...] DEST\n\nCopy files and set attributes.\n\n-d	Act like mkdir -p\n-D	Create leading directories for DEST\n-g	Make copy belong to GROUP\n-m	Set permissions to MODE\n-o	Make copy belong to USER\n-p	Preserve timestamps\n-s	Call \"strip -p\"\n-v	Verbose"

#define HELP_mv "usage: mv [-finTv] SOURCE... DEST\n\n-f	Force copy by deleting destination file\n-i	Interactive, prompt before overwriting existing DEST\n-n	No clobber (don't overwrite DEST)\n-T	DEST always treated as file, max 2 arguments\n-v	Verbose"

#define HELP_cp_preserve "--preserve takes either a comma separated list of attributes, or the first\nletter(s) of:\n\n        mode - permissions (ignore umask for rwx, copy suid and sticky bit)\n   ownership - user and group\n  timestamps - file creation, modification, and access times.\n     context - security context\n       xattr - extended attributes\n         all - all of the above\n\nusage: cp [--preserve=motcxa] [-adfHiLlnPpRrsTv] SOURCE... DEST\n\nCopy files from SOURCE to DEST.  If more than one SOURCE, DEST must\nbe a directory.\n-v	Verbose\n-T	DEST always treated as file, max 2 arguments\n-s	Symlink instead of copy\n-r	Synonym for -R\n-R	Recurse into subdirectories (DEST must be a directory)\n-p	Preserve timestamps, ownership, and mode\n-P	Do not follow symlinks [default]\n-n	No clobber (don't overwrite DEST)\n-l	Hard link instead of copy\n-L	Follow all symlinks\n-i	Interactive, prompt before overwriting existing DEST\n-H	Follow symlinks listed on command line\n-f	Delete destination files we can't write to\n-F	Delete any existing destination file first (--remove-destination)\n-d	Don't dereference symlinks\n-D	Create leading dirs under DEST (--parents)\n-a	Same as -dpr"

#define HELP_cp "usage: cp [--preserve=motcxa] [-adfHiLlnPpRrsTv] SOURCE... DEST\n\nCopy files from SOURCE to DEST.  If more than one SOURCE, DEST must\nbe a directory.\n-v	Verbose\n-T	DEST always treated as file, max 2 arguments\n-s	Symlink instead of copy\n-r	Synonym for -R\n-R	Recurse into subdirectories (DEST must be a directory)\n-p	Preserve timestamps, ownership, and mode\n-P	Do not follow symlinks [default]\n-n	No clobber (don't overwrite DEST)\n-l	Hard link instead of copy\n-L	Follow all symlinks\n-i	Interactive, prompt before overwriting existing DEST\n-H	Follow symlinks listed on command line\n-f	Delete destination files we can't write to\n-F	Delete any existing destination file first (--remove-destination)\n-d	Don't dereference symlinks\n-D	Create leading dirs under DEST (--parents)\n-a	Same as -dpr\n--preserve takes either a comma separated list of attributes, or the first\nletter(s) of:\n\n        mode - permissions (ignore umask for rwx, copy suid and sticky bit)\n   ownership - user and group\n  timestamps - file creation, modification, and access times.\n     context - security context\n       xattr - extended attributes\n         all - all of the above"

#define HELP_comm "usage: comm [-123] FILE1 FILE2\n\nRead FILE1 and FILE2, which should be ordered, and produce three text\ncolumns as output: lines only in FILE1; lines only in FILE2; and lines\nin both files. Filename \"-\" is a synonym for stdin.\n\n-1	Suppress the output column of lines unique to FILE1\n-2	Suppress the output column of lines unique to FILE2\n-3	Suppress the output column of lines duplicated in FILE1 and FILE2"

#define HELP_cmp "usage: cmp [-l] [-s] FILE1 [FILE2 [SKIP1 [SKIP2]]]\n\nCompare the contents of two files. (Or stdin and file if only one given.)\n\n-l	Show all differing bytes\n-s	Silent"

#define HELP_crc32 "usage: crc32 [file...]\n\nOutput crc32 checksum for each file."

#define HELP_cksum "usage: cksum [-IPLN] [FILE...]\n\nFor each file, output crc32 checksum value, length and name of file.\nIf no files listed, copy from stdin.  Filename \"-\" is a synonym for stdin.\n\n-H	Hexadecimal checksum (defaults to decimal)\n-L	Little endian (defaults to big endian)\n-P	Pre-inversion\n-I	Skip post-inversion\n-N	Do not include length in CRC calculation (or output)"

#define HELP_chmod "usage: chmod [-R] MODE FILE...\n\nChange mode of listed file[s] (recursively with -R).\n\nMODE can be (comma-separated) stanzas: [ugoa][+-=][rwxstXugo]\n\nStanzas are applied in order: For each category (u = user,\ng = group, o = other, a = all three, if none specified default is a),\nset (+), clear (-), or copy (=), r = read, w = write, x = execute.\ns = u+s = suid, g+s = sgid, o+s = sticky. (+t is an alias for o+s).\nsuid/sgid: execute as the user/group who owns the file.\nsticky: can't delete files you don't own out of this directory\nX = x for directories or if any category already has x set.\n\nOr MODE can be an octal value up to 7777	ug uuugggooo	top +\nbit 1 = o+x, bit 1<<8 = u+w, 1<<11 = g+1	sstrwxrwxrwx	bottom\n\nExamples:\nchmod u+w file - allow owner of \"file\" to write to it.\nchmod 744 file - user can read/write/execute, everyone else read only"

#define HELP_chown "see: chgrp"

#define HELP_chgrp "usage: chgrp/chown [-RHLP] [-fvh] GROUP FILE...\n\nChange group of one or more files.\n\n-f	Suppress most error messages\n-h	Change symlinks instead of what they point to\n-R	Recurse into subdirectories (implies -h)\n-H	With -R change target of symlink, follow command line symlinks\n-L	With -R change target of symlink, follow all symlinks\n-P	With -R change symlink, do not follow symlinks (default)\n-v	Verbose"

#define HELP_catv "usage: catv [-evt] [FILE...]\n\nDisplay nonprinting characters as escape sequences. Use M-x for\nhigh ascii characters (>127), and ^x for other nonprinting chars.\n\n-e	Mark each newline with $\n-t	Show tabs as ^I\n-v	Don't use ^x or M-x escapes"

#define HELP_cat "usage: cat [-etuv] [FILE...]\n\nCopy (concatenate) files to stdout.  If no files listed, copy from stdin.\nFilename \"-\" is a synonym for stdin.\n\n-e	Mark each newline with $\n-t	Show tabs as ^I\n-u	Copy one byte at a time (slow)\n-v	Display nonprinting characters as escape sequences with M-x for\n	high ascii characters (>127), and ^x for other nonprinting chars"

#define HELP_cal "usage: cal [[MONTH] YEAR]\n\nPrint a calendar.\n\nWith one argument, prints all months of the specified year.\nWith two arguments, prints calendar for month and year.\n\n-h	Don't highlight today"

#define HELP_basename "usage: basename [-a] [-s SUFFIX] NAME... | NAME [SUFFIX]\n\nReturn non-directory portion of a pathname removing suffix.\n\n-a		All arguments are names\n-s SUFFIX	Remove suffix (implies -a)"

