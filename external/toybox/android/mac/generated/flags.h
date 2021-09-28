#undef FORCED_FLAG
#undef FORCED_FLAGLL
#ifdef FORCE_FLAGS
#define FORCED_FLAG 1
#define FORCED_FLAGLL 1ULL
#else
#define FORCED_FLAG 0
#define FORCED_FLAGLL 0
#endif

// acpi   abctV
#undef OPTSTR_acpi
#define OPTSTR_acpi "abctV"
#ifdef CLEANUP_acpi
#undef CLEANUP_acpi
#undef FOR_acpi
#undef FLAG_V
#undef FLAG_t
#undef FLAG_c
#undef FLAG_b
#undef FLAG_a
#endif

// arch    
#undef OPTSTR_arch
#define OPTSTR_arch 0
#ifdef CLEANUP_arch
#undef CLEANUP_arch
#undef FOR_arch
#endif

// arp   vi:nDsdap:A:H:[+Ap][!sd]
#undef OPTSTR_arp
#define OPTSTR_arp "vi:nDsdap:A:H:[+Ap][!sd]"
#ifdef CLEANUP_arp
#undef CLEANUP_arp
#undef FOR_arp
#undef FLAG_H
#undef FLAG_A
#undef FLAG_p
#undef FLAG_a
#undef FLAG_d
#undef FLAG_s
#undef FLAG_D
#undef FLAG_n
#undef FLAG_i
#undef FLAG_v
#endif

// arping   <1>1s:I:w#<0c#<0AUDbqf[+AU][+Df]
#undef OPTSTR_arping
#define OPTSTR_arping "<1>1s:I:w#<0c#<0AUDbqf[+AU][+Df]"
#ifdef CLEANUP_arping
#undef CLEANUP_arping
#undef FOR_arping
#undef FLAG_f
#undef FLAG_q
#undef FLAG_b
#undef FLAG_D
#undef FLAG_U
#undef FLAG_A
#undef FLAG_c
#undef FLAG_w
#undef FLAG_I
#undef FLAG_s
#endif

// ascii    
#undef OPTSTR_ascii
#define OPTSTR_ascii 0
#ifdef CLEANUP_ascii
#undef CLEANUP_ascii
#undef FOR_ascii
#endif

// base64   diw#<0=76[!dw]
#undef OPTSTR_base64
#define OPTSTR_base64 "diw#<0=76[!dw]"
#ifdef CLEANUP_base64
#undef CLEANUP_base64
#undef FOR_base64
#undef FLAG_w
#undef FLAG_i
#undef FLAG_d
#endif

// basename ^<1as: ^<1as:
#undef OPTSTR_basename
#define OPTSTR_basename "^<1as:"
#ifdef CLEANUP_basename
#undef CLEANUP_basename
#undef FOR_basename
#undef FLAG_s
#undef FLAG_a
#endif

// bc   i(interactive)l(mathlib)q(quiet)s(standard)w(warn)
#undef OPTSTR_bc
#define OPTSTR_bc "i(interactive)l(mathlib)q(quiet)s(standard)w(warn)"
#ifdef CLEANUP_bc
#undef CLEANUP_bc
#undef FOR_bc
#undef FLAG_w
#undef FLAG_s
#undef FLAG_q
#undef FLAG_l
#undef FLAG_i
#endif

// blkid   ULs*[!LU]
#undef OPTSTR_blkid
#define OPTSTR_blkid "ULs*[!LU]"
#ifdef CLEANUP_blkid
#undef CLEANUP_blkid
#undef FOR_blkid
#undef FLAG_s
#undef FLAG_L
#undef FLAG_U
#endif

// blockdev   <1>1(setro)(setrw)(getro)(getss)(getbsz)(setbsz)#<0(getsz)(getsize)(getsize64)(getra)(setra)#<0(flushbufs)(rereadpt)
#undef OPTSTR_blockdev
#define OPTSTR_blockdev "<1>1(setro)(setrw)(getro)(getss)(getbsz)(setbsz)#<0(getsz)(getsize)(getsize64)(getra)(setra)#<0(flushbufs)(rereadpt)"
#ifdef CLEANUP_blockdev
#undef CLEANUP_blockdev
#undef FOR_blockdev
#undef FLAG_rereadpt
#undef FLAG_flushbufs
#undef FLAG_setra
#undef FLAG_getra
#undef FLAG_getsize64
#undef FLAG_getsize
#undef FLAG_getsz
#undef FLAG_setbsz
#undef FLAG_getbsz
#undef FLAG_getss
#undef FLAG_getro
#undef FLAG_setrw
#undef FLAG_setro
#endif

// bootchartd    
#undef OPTSTR_bootchartd
#define OPTSTR_bootchartd 0
#ifdef CLEANUP_bootchartd
#undef CLEANUP_bootchartd
#undef FOR_bootchartd
#endif

// brctl   <1
#undef OPTSTR_brctl
#define OPTSTR_brctl "<1"
#ifdef CLEANUP_brctl
#undef CLEANUP_brctl
#undef FOR_brctl
#endif

// bunzip2   cftkv
#undef OPTSTR_bunzip2
#define OPTSTR_bunzip2 "cftkv"
#ifdef CLEANUP_bunzip2
#undef CLEANUP_bunzip2
#undef FOR_bunzip2
#undef FLAG_v
#undef FLAG_k
#undef FLAG_t
#undef FLAG_f
#undef FLAG_c
#endif

// bzcat    
#undef OPTSTR_bzcat
#define OPTSTR_bzcat 0
#ifdef CLEANUP_bzcat
#undef CLEANUP_bzcat
#undef FOR_bzcat
#endif

// cal   >2h
#undef OPTSTR_cal
#define OPTSTR_cal ">2h"
#ifdef CLEANUP_cal
#undef CLEANUP_cal
#undef FOR_cal
#undef FLAG_h
#endif

// cat uvte uvte
#undef OPTSTR_cat
#define OPTSTR_cat "uvte"
#ifdef CLEANUP_cat
#undef CLEANUP_cat
#undef FOR_cat
#undef FLAG_e
#undef FLAG_t
#undef FLAG_v
#undef FLAG_u
#endif

// catv   vte
#undef OPTSTR_catv
#define OPTSTR_catv "vte"
#ifdef CLEANUP_catv
#undef CLEANUP_catv
#undef FOR_catv
#undef FLAG_e
#undef FLAG_t
#undef FLAG_v
#endif

// cd   >1LP[-LP]
#undef OPTSTR_cd
#define OPTSTR_cd ">1LP[-LP]"
#ifdef CLEANUP_cd
#undef CLEANUP_cd
#undef FOR_cd
#undef FLAG_P
#undef FLAG_L
#endif

// chattr   ?p#v#R
#undef OPTSTR_chattr
#define OPTSTR_chattr "?p#v#R"
#ifdef CLEANUP_chattr
#undef CLEANUP_chattr
#undef FOR_chattr
#undef FLAG_R
#undef FLAG_v
#undef FLAG_p
#endif

// chcon   <2hvR
#undef OPTSTR_chcon
#define OPTSTR_chcon "<2hvR"
#ifdef CLEANUP_chcon
#undef CLEANUP_chcon
#undef FOR_chcon
#undef FLAG_R
#undef FLAG_v
#undef FLAG_h
#endif

// chgrp   <2hPLHRfv[-HLP]
#undef OPTSTR_chgrp
#define OPTSTR_chgrp "<2hPLHRfv[-HLP]"
#ifdef CLEANUP_chgrp
#undef CLEANUP_chgrp
#undef FOR_chgrp
#undef FLAG_v
#undef FLAG_f
#undef FLAG_R
#undef FLAG_H
#undef FLAG_L
#undef FLAG_P
#undef FLAG_h
#endif

// chmod <2?vRf[-vf] <2?vRf[-vf]
#undef OPTSTR_chmod
#define OPTSTR_chmod "<2?vRf[-vf]"
#ifdef CLEANUP_chmod
#undef CLEANUP_chmod
#undef FOR_chmod
#undef FLAG_f
#undef FLAG_R
#undef FLAG_v
#endif

// chroot   ^<1
#undef OPTSTR_chroot
#define OPTSTR_chroot "^<1"
#ifdef CLEANUP_chroot
#undef CLEANUP_chroot
#undef FOR_chroot
#endif

// chrt   ^mp#<0iRbrfo[!ibrfo]
#undef OPTSTR_chrt
#define OPTSTR_chrt "^mp#<0iRbrfo[!ibrfo]"
#ifdef CLEANUP_chrt
#undef CLEANUP_chrt
#undef FOR_chrt
#undef FLAG_o
#undef FLAG_f
#undef FLAG_r
#undef FLAG_b
#undef FLAG_R
#undef FLAG_i
#undef FLAG_p
#undef FLAG_m
#endif

// chvt   <1
#undef OPTSTR_chvt
#define OPTSTR_chvt "<1"
#ifdef CLEANUP_chvt
#undef CLEANUP_chvt
#undef FOR_chvt
#endif

// cksum   HIPLN
#undef OPTSTR_cksum
#define OPTSTR_cksum "HIPLN"
#ifdef CLEANUP_cksum
#undef CLEANUP_cksum
#undef FOR_cksum
#undef FLAG_N
#undef FLAG_L
#undef FLAG_P
#undef FLAG_I
#undef FLAG_H
#endif

// clear    
#undef OPTSTR_clear
#define OPTSTR_clear 0
#ifdef CLEANUP_clear
#undef CLEANUP_clear
#undef FOR_clear
#endif

// cmp <1>2ls(silent)(quiet)[!ls] <1>2ls(silent)(quiet)[!ls]
#undef OPTSTR_cmp
#define OPTSTR_cmp "<1>2ls(silent)(quiet)[!ls]"
#ifdef CLEANUP_cmp
#undef CLEANUP_cmp
#undef FOR_cmp
#undef FLAG_s
#undef FLAG_l
#endif

// comm <2>2321 <2>2321
#undef OPTSTR_comm
#define OPTSTR_comm "<2>2321"
#ifdef CLEANUP_comm
#undef CLEANUP_comm
#undef FOR_comm
#undef FLAG_1
#undef FLAG_2
#undef FLAG_3
#endif

// count    
#undef OPTSTR_count
#define OPTSTR_count 0
#ifdef CLEANUP_count
#undef CLEANUP_count
#undef FOR_count
#endif

// cp <2(preserve):;D(parents)RHLPprdaslvnF(remove-destination)fiT[-HLPd][-ni] <2(preserve):;D(parents)RHLPprdaslvnF(remove-destination)fiT[-HLPd][-ni]
#undef OPTSTR_cp
#define OPTSTR_cp "<2(preserve):;D(parents)RHLPprdaslvnF(remove-destination)fiT[-HLPd][-ni]"
#ifdef CLEANUP_cp
#undef CLEANUP_cp
#undef FOR_cp
#undef FLAG_T
#undef FLAG_i
#undef FLAG_f
#undef FLAG_F
#undef FLAG_n
#undef FLAG_v
#undef FLAG_l
#undef FLAG_s
#undef FLAG_a
#undef FLAG_d
#undef FLAG_r
#undef FLAG_p
#undef FLAG_P
#undef FLAG_L
#undef FLAG_H
#undef FLAG_R
#undef FLAG_D
#undef FLAG_preserve
#endif

// cpio   (no-preserve-owner)(trailer)mduH:p:|i|t|F:v(verbose)o|[!pio][!pot][!pF]
#undef OPTSTR_cpio
#define OPTSTR_cpio "(no-preserve-owner)(trailer)mduH:p:|i|t|F:v(verbose)o|[!pio][!pot][!pF]"
#ifdef CLEANUP_cpio
#undef CLEANUP_cpio
#undef FOR_cpio
#undef FLAG_o
#undef FLAG_v
#undef FLAG_F
#undef FLAG_t
#undef FLAG_i
#undef FLAG_p
#undef FLAG_H
#undef FLAG_u
#undef FLAG_d
#undef FLAG_m
#undef FLAG_trailer
#undef FLAG_no_preserve_owner
#endif

// crc32    
#undef OPTSTR_crc32
#define OPTSTR_crc32 0
#ifdef CLEANUP_crc32
#undef CLEANUP_crc32
#undef FOR_crc32
#endif

// crond   fbSl#<0=8d#<0L:c:[-bf][-LS][-ld]
#undef OPTSTR_crond
#define OPTSTR_crond "fbSl#<0=8d#<0L:c:[-bf][-LS][-ld]"
#ifdef CLEANUP_crond
#undef CLEANUP_crond
#undef FOR_crond
#undef FLAG_c
#undef FLAG_L
#undef FLAG_d
#undef FLAG_l
#undef FLAG_S
#undef FLAG_b
#undef FLAG_f
#endif

// crontab   c:u:elr[!elr]
#undef OPTSTR_crontab
#define OPTSTR_crontab "c:u:elr[!elr]"
#ifdef CLEANUP_crontab
#undef CLEANUP_crontab
#undef FOR_crontab
#undef FLAG_r
#undef FLAG_l
#undef FLAG_e
#undef FLAG_u
#undef FLAG_c
#endif

// cut b*|c*|f*|F*|C*|O(output-delimiter):d:sDn[!cbf] b*|c*|f*|F*|C*|O(output-delimiter):d:sDn[!cbf]
#undef OPTSTR_cut
#define OPTSTR_cut "b*|c*|f*|F*|C*|O(output-delimiter):d:sDn[!cbf]"
#ifdef CLEANUP_cut
#undef CLEANUP_cut
#undef FOR_cut
#undef FLAG_n
#undef FLAG_D
#undef FLAG_s
#undef FLAG_d
#undef FLAG_O
#undef FLAG_C
#undef FLAG_F
#undef FLAG_f
#undef FLAG_c
#undef FLAG_b
#endif

// date d:D:r:u[!dr] d:D:r:u[!dr]
#undef OPTSTR_date
#define OPTSTR_date "d:D:r:u[!dr]"
#ifdef CLEANUP_date
#undef CLEANUP_date
#undef FOR_date
#undef FLAG_u
#undef FLAG_r
#undef FLAG_D
#undef FLAG_d
#endif

// dd    
#undef OPTSTR_dd
#define OPTSTR_dd 0
#ifdef CLEANUP_dd
#undef CLEANUP_dd
#undef FOR_dd
#endif

// deallocvt   >1
#undef OPTSTR_deallocvt
#define OPTSTR_deallocvt ">1"
#ifdef CLEANUP_deallocvt
#undef CLEANUP_deallocvt
#undef FOR_deallocvt
#endif

// demo_many_options   ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba
#undef OPTSTR_demo_many_options
#define OPTSTR_demo_many_options "ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
#ifdef CLEANUP_demo_many_options
#undef CLEANUP_demo_many_options
#undef FOR_demo_many_options
#undef FLAG_a
#undef FLAG_b
#undef FLAG_c
#undef FLAG_d
#undef FLAG_e
#undef FLAG_f
#undef FLAG_g
#undef FLAG_h
#undef FLAG_i
#undef FLAG_j
#undef FLAG_k
#undef FLAG_l
#undef FLAG_m
#undef FLAG_n
#undef FLAG_o
#undef FLAG_p
#undef FLAG_q
#undef FLAG_r
#undef FLAG_s
#undef FLAG_t
#undef FLAG_u
#undef FLAG_v
#undef FLAG_w
#undef FLAG_x
#undef FLAG_y
#undef FLAG_z
#undef FLAG_A
#undef FLAG_B
#undef FLAG_C
#undef FLAG_D
#undef FLAG_E
#undef FLAG_F
#undef FLAG_G
#undef FLAG_H
#undef FLAG_I
#undef FLAG_J
#undef FLAG_K
#undef FLAG_L
#undef FLAG_M
#undef FLAG_N
#undef FLAG_O
#undef FLAG_P
#undef FLAG_Q
#undef FLAG_R
#undef FLAG_S
#undef FLAG_T
#undef FLAG_U
#undef FLAG_V
#undef FLAG_W
#undef FLAG_X
#undef FLAG_Y
#undef FLAG_Z
#endif

// demo_number   D#=3<3hdbs
#undef OPTSTR_demo_number
#define OPTSTR_demo_number "D#=3<3hdbs"
#ifdef CLEANUP_demo_number
#undef CLEANUP_demo_number
#undef FOR_demo_number
#undef FLAG_s
#undef FLAG_b
#undef FLAG_d
#undef FLAG_h
#undef FLAG_D
#endif

// demo_scankey    
#undef OPTSTR_demo_scankey
#define OPTSTR_demo_scankey 0
#ifdef CLEANUP_demo_scankey
#undef CLEANUP_demo_scankey
#undef FOR_demo_scankey
#endif

// demo_utf8towc    
#undef OPTSTR_demo_utf8towc
#define OPTSTR_demo_utf8towc 0
#ifdef CLEANUP_demo_utf8towc
#undef CLEANUP_demo_utf8towc
#undef FOR_demo_utf8towc
#endif

// devmem   <1>3
#undef OPTSTR_devmem
#define OPTSTR_devmem "<1>3"
#ifdef CLEANUP_devmem
#undef CLEANUP_devmem
#undef FOR_devmem
#endif

// df   HPkhit*a[-HPkh]
#undef OPTSTR_df
#define OPTSTR_df "HPkhit*a[-HPkh]"
#ifdef CLEANUP_df
#undef CLEANUP_df
#undef FOR_df
#undef FLAG_a
#undef FLAG_t
#undef FLAG_i
#undef FLAG_h
#undef FLAG_k
#undef FLAG_P
#undef FLAG_H
#endif

// dhcp   V:H:F:x*r:O*A#<0=20T#<0=3t#<0=3s:p:i:SBRCaovqnbf
#undef OPTSTR_dhcp
#define OPTSTR_dhcp "V:H:F:x*r:O*A#<0=20T#<0=3t#<0=3s:p:i:SBRCaovqnbf"
#ifdef CLEANUP_dhcp
#undef CLEANUP_dhcp
#undef FOR_dhcp
#undef FLAG_f
#undef FLAG_b
#undef FLAG_n
#undef FLAG_q
#undef FLAG_v
#undef FLAG_o
#undef FLAG_a
#undef FLAG_C
#undef FLAG_R
#undef FLAG_B
#undef FLAG_S
#undef FLAG_i
#undef FLAG_p
#undef FLAG_s
#undef FLAG_t
#undef FLAG_T
#undef FLAG_A
#undef FLAG_O
#undef FLAG_r
#undef FLAG_x
#undef FLAG_F
#undef FLAG_H
#undef FLAG_V
#endif

// dhcp6   r:A#<0T#<0t#<0s:p:i:SRvqnbf
#undef OPTSTR_dhcp6
#define OPTSTR_dhcp6 "r:A#<0T#<0t#<0s:p:i:SRvqnbf"
#ifdef CLEANUP_dhcp6
#undef CLEANUP_dhcp6
#undef FOR_dhcp6
#undef FLAG_f
#undef FLAG_b
#undef FLAG_n
#undef FLAG_q
#undef FLAG_v
#undef FLAG_R
#undef FLAG_S
#undef FLAG_i
#undef FLAG_p
#undef FLAG_s
#undef FLAG_t
#undef FLAG_T
#undef FLAG_A
#undef FLAG_r
#endif

// dhcpd   >1P#<0>65535fi:S46[!46]
#undef OPTSTR_dhcpd
#define OPTSTR_dhcpd ">1P#<0>65535fi:S46[!46]"
#ifdef CLEANUP_dhcpd
#undef CLEANUP_dhcpd
#undef FOR_dhcpd
#undef FLAG_6
#undef FLAG_4
#undef FLAG_S
#undef FLAG_i
#undef FLAG_f
#undef FLAG_P
#endif

// diff <2>2(color)(strip-trailing-cr)B(ignore-blank-lines)d(minimal)b(ignore-space-change)ut(expand-tabs)w(ignore-all-space)i(ignore-case)T(initial-tab)s(report-identical-files)q(brief)a(text)L(label)*S(starting-file):N(new-file)r(recursive)U(unified)#<0=3 <2>2(color)(strip-trailing-cr)B(ignore-blank-lines)d(minimal)b(ignore-space-change)ut(expand-tabs)w(ignore-all-space)i(ignore-case)T(initial-tab)s(report-identical-files)q(brief)a(text)L(label)*S(starting-file):N(new-file)r(recursive)U(unified)#<0=3
#undef OPTSTR_diff
#define OPTSTR_diff "<2>2(color)(strip-trailing-cr)B(ignore-blank-lines)d(minimal)b(ignore-space-change)ut(expand-tabs)w(ignore-all-space)i(ignore-case)T(initial-tab)s(report-identical-files)q(brief)a(text)L(label)*S(starting-file):N(new-file)r(recursive)U(unified)#<0=3"
#ifdef CLEANUP_diff
#undef CLEANUP_diff
#undef FOR_diff
#undef FLAG_U
#undef FLAG_r
#undef FLAG_N
#undef FLAG_S
#undef FLAG_L
#undef FLAG_a
#undef FLAG_q
#undef FLAG_s
#undef FLAG_T
#undef FLAG_i
#undef FLAG_w
#undef FLAG_t
#undef FLAG_u
#undef FLAG_b
#undef FLAG_d
#undef FLAG_B
#undef FLAG_strip_trailing_cr
#undef FLAG_color
#endif

// dirname <1 <1
#undef OPTSTR_dirname
#define OPTSTR_dirname "<1"
#ifdef CLEANUP_dirname
#undef CLEANUP_dirname
#undef FOR_dirname
#endif

// dmesg   w(follow)CSTtrs#<1n#c[!Ttr][!Cc][!Sw]
#undef OPTSTR_dmesg
#define OPTSTR_dmesg "w(follow)CSTtrs#<1n#c[!Ttr][!Cc][!Sw]"
#ifdef CLEANUP_dmesg
#undef CLEANUP_dmesg
#undef FOR_dmesg
#undef FLAG_c
#undef FLAG_n
#undef FLAG_s
#undef FLAG_r
#undef FLAG_t
#undef FLAG_T
#undef FLAG_S
#undef FLAG_C
#undef FLAG_w
#endif

// dnsdomainname   >0
#undef OPTSTR_dnsdomainname
#define OPTSTR_dnsdomainname ">0"
#ifdef CLEANUP_dnsdomainname
#undef CLEANUP_dnsdomainname
#undef FOR_dnsdomainname
#endif

// dos2unix    
#undef OPTSTR_dos2unix
#define OPTSTR_dos2unix 0
#ifdef CLEANUP_dos2unix
#undef CLEANUP_dos2unix
#undef FOR_dos2unix
#endif

// du d#<0=-1hmlcaHkKLsx[-HL][-kKmh] d#<0=-1hmlcaHkKLsx[-HL][-kKmh]
#undef OPTSTR_du
#define OPTSTR_du "d#<0=-1hmlcaHkKLsx[-HL][-kKmh]"
#ifdef CLEANUP_du
#undef CLEANUP_du
#undef FOR_du
#undef FLAG_x
#undef FLAG_s
#undef FLAG_L
#undef FLAG_K
#undef FLAG_k
#undef FLAG_H
#undef FLAG_a
#undef FLAG_c
#undef FLAG_l
#undef FLAG_m
#undef FLAG_h
#undef FLAG_d
#endif

// dumpleases   >0arf:[!ar]
#undef OPTSTR_dumpleases
#define OPTSTR_dumpleases ">0arf:[!ar]"
#ifdef CLEANUP_dumpleases
#undef CLEANUP_dumpleases
#undef FOR_dumpleases
#undef FLAG_f
#undef FLAG_r
#undef FLAG_a
#endif

// echo ^?Een[-eE] ^?Een[-eE]
#undef OPTSTR_echo
#define OPTSTR_echo "^?Een[-eE]"
#ifdef CLEANUP_echo
#undef CLEANUP_echo
#undef FOR_echo
#undef FLAG_n
#undef FLAG_e
#undef FLAG_E
#endif

// eject   >1stT[!tT]
#undef OPTSTR_eject
#define OPTSTR_eject ">1stT[!tT]"
#ifdef CLEANUP_eject
#undef CLEANUP_eject
#undef FOR_eject
#undef FLAG_T
#undef FLAG_t
#undef FLAG_s
#endif

// env ^0iu* ^0iu*
#undef OPTSTR_env
#define OPTSTR_env "^0iu*"
#ifdef CLEANUP_env
#undef CLEANUP_env
#undef FOR_env
#undef FLAG_u
#undef FLAG_i
#undef FLAG_0
#endif

// exit    
#undef OPTSTR_exit
#define OPTSTR_exit 0
#ifdef CLEANUP_exit
#undef CLEANUP_exit
#undef FOR_exit
#endif

// expand   t*
#undef OPTSTR_expand
#define OPTSTR_expand "t*"
#ifdef CLEANUP_expand
#undef CLEANUP_expand
#undef FOR_expand
#undef FLAG_t
#endif

// expr    
#undef OPTSTR_expr
#define OPTSTR_expr 0
#ifdef CLEANUP_expr
#undef CLEANUP_expr
#undef FOR_expr
#endif

// factor    
#undef OPTSTR_factor
#define OPTSTR_factor 0
#ifdef CLEANUP_factor
#undef CLEANUP_factor
#undef FOR_factor
#endif

// fallocate   >1l#|o#
#undef OPTSTR_fallocate
#define OPTSTR_fallocate ">1l#|o#"
#ifdef CLEANUP_fallocate
#undef CLEANUP_fallocate
#undef FOR_fallocate
#undef FLAG_o
#undef FLAG_l
#endif

// false    
#undef OPTSTR_false
#define OPTSTR_false 0
#ifdef CLEANUP_false
#undef CLEANUP_false
#undef FOR_false
#endif

// fdisk   C#<0H#<0S#<0b#<512ul
#undef OPTSTR_fdisk
#define OPTSTR_fdisk "C#<0H#<0S#<0b#<512ul"
#ifdef CLEANUP_fdisk
#undef CLEANUP_fdisk
#undef FOR_fdisk
#undef FLAG_l
#undef FLAG_u
#undef FLAG_b
#undef FLAG_S
#undef FLAG_H
#undef FLAG_C
#endif

// file   <1bhLs[!hL]
#undef OPTSTR_file
#define OPTSTR_file "<1bhLs[!hL]"
#ifdef CLEANUP_file
#undef CLEANUP_file
#undef FOR_file
#undef FLAG_s
#undef FLAG_L
#undef FLAG_h
#undef FLAG_b
#endif

// find ?^HL[-HL] ?^HL[-HL]
#undef OPTSTR_find
#define OPTSTR_find "?^HL[-HL]"
#ifdef CLEANUP_find
#undef CLEANUP_find
#undef FOR_find
#undef FLAG_L
#undef FLAG_H
#endif

// flock   <1>1nsux[-sux]
#undef OPTSTR_flock
#define OPTSTR_flock "<1>1nsux[-sux]"
#ifdef CLEANUP_flock
#undef CLEANUP_flock
#undef FOR_flock
#undef FLAG_x
#undef FLAG_u
#undef FLAG_s
#undef FLAG_n
#endif

// fmt   w#<0=75
#undef OPTSTR_fmt
#define OPTSTR_fmt "w#<0=75"
#ifdef CLEANUP_fmt
#undef CLEANUP_fmt
#undef FOR_fmt
#undef FLAG_w
#endif

// fold   bsuw#<1
#undef OPTSTR_fold
#define OPTSTR_fold "bsuw#<1"
#ifdef CLEANUP_fold
#undef CLEANUP_fold
#undef FOR_fold
#undef FLAG_w
#undef FLAG_u
#undef FLAG_s
#undef FLAG_b
#endif

// free   htgmkb[!htgmkb]
#undef OPTSTR_free
#define OPTSTR_free "htgmkb[!htgmkb]"
#ifdef CLEANUP_free
#undef CLEANUP_free
#undef FOR_free
#undef FLAG_b
#undef FLAG_k
#undef FLAG_m
#undef FLAG_g
#undef FLAG_t
#undef FLAG_h
#endif

// freeramdisk   <1>1
#undef OPTSTR_freeramdisk
#define OPTSTR_freeramdisk "<1>1"
#ifdef CLEANUP_freeramdisk
#undef CLEANUP_freeramdisk
#undef FOR_freeramdisk
#endif

// fsck   ?t:ANPRTVsC#
#undef OPTSTR_fsck
#define OPTSTR_fsck "?t:ANPRTVsC#"
#ifdef CLEANUP_fsck
#undef CLEANUP_fsck
#undef FOR_fsck
#undef FLAG_C
#undef FLAG_s
#undef FLAG_V
#undef FLAG_T
#undef FLAG_R
#undef FLAG_P
#undef FLAG_N
#undef FLAG_A
#undef FLAG_t
#endif

// fsfreeze   <1>1f|u|[!fu]
#undef OPTSTR_fsfreeze
#define OPTSTR_fsfreeze "<1>1f|u|[!fu]"
#ifdef CLEANUP_fsfreeze
#undef CLEANUP_fsfreeze
#undef FOR_fsfreeze
#undef FLAG_u
#undef FLAG_f
#endif

// fstype   <1
#undef OPTSTR_fstype
#define OPTSTR_fstype "<1"
#ifdef CLEANUP_fstype
#undef CLEANUP_fstype
#undef FOR_fstype
#endif

// fsync   <1d
#undef OPTSTR_fsync
#define OPTSTR_fsync "<1d"
#ifdef CLEANUP_fsync
#undef CLEANUP_fsync
#undef FOR_fsync
#undef FLAG_d
#endif

// ftpget   <2>3P:cp:u:vgslLmMdD[-gs][!gslLmMdD][!clL]
#undef OPTSTR_ftpget
#define OPTSTR_ftpget "<2>3P:cp:u:vgslLmMdD[-gs][!gslLmMdD][!clL]"
#ifdef CLEANUP_ftpget
#undef CLEANUP_ftpget
#undef FOR_ftpget
#undef FLAG_D
#undef FLAG_d
#undef FLAG_M
#undef FLAG_m
#undef FLAG_L
#undef FLAG_l
#undef FLAG_s
#undef FLAG_g
#undef FLAG_v
#undef FLAG_u
#undef FLAG_p
#undef FLAG_c
#undef FLAG_P
#endif

// getconf >2al >2al
#undef OPTSTR_getconf
#define OPTSTR_getconf ">2al"
#ifdef CLEANUP_getconf
#undef CLEANUP_getconf
#undef FOR_getconf
#undef FLAG_l
#undef FLAG_a
#endif

// getenforce   >0
#undef OPTSTR_getenforce
#define OPTSTR_getenforce ">0"
#ifdef CLEANUP_getenforce
#undef CLEANUP_getenforce
#undef FOR_getenforce
#endif

// getfattr   (only-values)dhn:
#undef OPTSTR_getfattr
#define OPTSTR_getfattr "(only-values)dhn:"
#ifdef CLEANUP_getfattr
#undef CLEANUP_getfattr
#undef FOR_getfattr
#undef FLAG_n
#undef FLAG_h
#undef FLAG_d
#undef FLAG_only_values
#endif

// getopt ^a(alternative)n:(name)o:(options)l*(long)(longoptions)Tu ^a(alternative)n:(name)o:(options)l*(long)(longoptions)Tu
#undef OPTSTR_getopt
#define OPTSTR_getopt "^a(alternative)n:(name)o:(options)l*(long)(longoptions)Tu"
#ifdef CLEANUP_getopt
#undef CLEANUP_getopt
#undef FOR_getopt
#undef FLAG_u
#undef FLAG_T
#undef FLAG_l
#undef FLAG_o
#undef FLAG_n
#undef FLAG_a
#endif

// getty   <2t#<0H:I:l:f:iwnmLh
#undef OPTSTR_getty
#define OPTSTR_getty "<2t#<0H:I:l:f:iwnmLh"
#ifdef CLEANUP_getty
#undef CLEANUP_getty
#undef FOR_getty
#undef FLAG_h
#undef FLAG_L
#undef FLAG_m
#undef FLAG_n
#undef FLAG_w
#undef FLAG_i
#undef FLAG_f
#undef FLAG_l
#undef FLAG_I
#undef FLAG_H
#undef FLAG_t
#endif

// grep (line-buffered)(color):;(exclude-dir)*S(exclude)*M(include)*ZzEFHIab(byte-offset)h(no-filename)ino(only-matching)rRsvwcl(files-with-matches)q(quiet)(silent)e*f*C#B#A#m#x[!wx][!EFw] (line-buffered)(color):;(exclude-dir)*S(exclude)*M(include)*ZzEFHIab(byte-offset)h(no-filename)ino(only-matching)rRsvwcl(files-with-matches)q(quiet)(silent)e*f*C#B#A#m#x[!wx][!EFw]
#undef OPTSTR_grep
#define OPTSTR_grep "(line-buffered)(color):;(exclude-dir)*S(exclude)*M(include)*ZzEFHIab(byte-offset)h(no-filename)ino(only-matching)rRsvwcl(files-with-matches)q(quiet)(silent)e*f*C#B#A#m#x[!wx][!EFw]"
#ifdef CLEANUP_grep
#undef CLEANUP_grep
#undef FOR_grep
#undef FLAG_x
#undef FLAG_m
#undef FLAG_A
#undef FLAG_B
#undef FLAG_C
#undef FLAG_f
#undef FLAG_e
#undef FLAG_q
#undef FLAG_l
#undef FLAG_c
#undef FLAG_w
#undef FLAG_v
#undef FLAG_s
#undef FLAG_R
#undef FLAG_r
#undef FLAG_o
#undef FLAG_n
#undef FLAG_i
#undef FLAG_h
#undef FLAG_b
#undef FLAG_a
#undef FLAG_I
#undef FLAG_H
#undef FLAG_F
#undef FLAG_E
#undef FLAG_z
#undef FLAG_Z
#undef FLAG_M
#undef FLAG_S
#undef FLAG_exclude_dir
#undef FLAG_color
#undef FLAG_line_buffered
#endif

// groupadd   <1>2g#<0S
#undef OPTSTR_groupadd
#define OPTSTR_groupadd "<1>2g#<0S"
#ifdef CLEANUP_groupadd
#undef CLEANUP_groupadd
#undef FOR_groupadd
#undef FLAG_S
#undef FLAG_g
#endif

// groupdel   <1>2
#undef OPTSTR_groupdel
#define OPTSTR_groupdel "<1>2"
#ifdef CLEANUP_groupdel
#undef CLEANUP_groupdel
#undef FOR_groupdel
#endif

// groups    
#undef OPTSTR_groups
#define OPTSTR_groups 0
#ifdef CLEANUP_groups
#undef CLEANUP_groups
#undef FOR_groups
#endif

// gunzip   cdfk123456789[-123456789]
#undef OPTSTR_gunzip
#define OPTSTR_gunzip "cdfk123456789[-123456789]"
#ifdef CLEANUP_gunzip
#undef CLEANUP_gunzip
#undef FOR_gunzip
#undef FLAG_9
#undef FLAG_8
#undef FLAG_7
#undef FLAG_6
#undef FLAG_5
#undef FLAG_4
#undef FLAG_3
#undef FLAG_2
#undef FLAG_1
#undef FLAG_k
#undef FLAG_f
#undef FLAG_d
#undef FLAG_c
#endif

// gzip ncdfk123456789[-123456789] ncdfk123456789[-123456789]
#undef OPTSTR_gzip
#define OPTSTR_gzip "ncdfk123456789[-123456789]"
#ifdef CLEANUP_gzip
#undef CLEANUP_gzip
#undef FOR_gzip
#undef FLAG_9
#undef FLAG_8
#undef FLAG_7
#undef FLAG_6
#undef FLAG_5
#undef FLAG_4
#undef FLAG_3
#undef FLAG_2
#undef FLAG_1
#undef FLAG_k
#undef FLAG_f
#undef FLAG_d
#undef FLAG_c
#undef FLAG_n
#endif

// head ?n(lines)#<0=10c(bytes)#<0qv[-nc] ?n(lines)#<0=10c(bytes)#<0qv[-nc]
#undef OPTSTR_head
#define OPTSTR_head "?n(lines)#<0=10c(bytes)#<0qv[-nc]"
#ifdef CLEANUP_head
#undef CLEANUP_head
#undef FOR_head
#undef FLAG_v
#undef FLAG_q
#undef FLAG_c
#undef FLAG_n
#endif

// hello    
#undef OPTSTR_hello
#define OPTSTR_hello 0
#ifdef CLEANUP_hello
#undef CLEANUP_hello
#undef FOR_hello
#endif

// help   ahu
#undef OPTSTR_help
#define OPTSTR_help "ahu"
#ifdef CLEANUP_help
#undef CLEANUP_help
#undef FOR_help
#undef FLAG_u
#undef FLAG_h
#undef FLAG_a
#endif

// hexedit   <1>1r
#undef OPTSTR_hexedit
#define OPTSTR_hexedit "<1>1r"
#ifdef CLEANUP_hexedit
#undef CLEANUP_hexedit
#undef FOR_hexedit
#undef FLAG_r
#endif

// host   <1>2avt:
#undef OPTSTR_host
#define OPTSTR_host "<1>2avt:"
#ifdef CLEANUP_host
#undef CLEANUP_host
#undef FOR_host
#undef FLAG_t
#undef FLAG_v
#undef FLAG_a
#endif

// hostid   >0
#undef OPTSTR_hostid
#define OPTSTR_hostid ">0"
#ifdef CLEANUP_hostid
#undef CLEANUP_hostid
#undef FOR_hostid
#endif

// hostname >1bdsfF:[!bdsf] >1bdsfF:[!bdsf]
#undef OPTSTR_hostname
#define OPTSTR_hostname ">1bdsfF:[!bdsf]"
#ifdef CLEANUP_hostname
#undef CLEANUP_hostname
#undef FOR_hostname
#undef FLAG_F
#undef FLAG_f
#undef FLAG_s
#undef FLAG_d
#undef FLAG_b
#endif

// hwclock   >0(fast)f(rtc):u(utc)l(localtime)t(systz)s(hctosys)r(show)w(systohc)[-ul][!rtsw]
#undef OPTSTR_hwclock
#define OPTSTR_hwclock ">0(fast)f(rtc):u(utc)l(localtime)t(systz)s(hctosys)r(show)w(systohc)[-ul][!rtsw]"
#ifdef CLEANUP_hwclock
#undef CLEANUP_hwclock
#undef FOR_hwclock
#undef FLAG_w
#undef FLAG_r
#undef FLAG_s
#undef FLAG_t
#undef FLAG_l
#undef FLAG_u
#undef FLAG_f
#undef FLAG_fast
#endif

// i2cdetect   >3aFly
#undef OPTSTR_i2cdetect
#define OPTSTR_i2cdetect ">3aFly"
#ifdef CLEANUP_i2cdetect
#undef CLEANUP_i2cdetect
#undef FOR_i2cdetect
#undef FLAG_y
#undef FLAG_l
#undef FLAG_F
#undef FLAG_a
#endif

// i2cdump   <2>2fy
#undef OPTSTR_i2cdump
#define OPTSTR_i2cdump "<2>2fy"
#ifdef CLEANUP_i2cdump
#undef CLEANUP_i2cdump
#undef FOR_i2cdump
#undef FLAG_y
#undef FLAG_f
#endif

// i2cget   <3>3fy
#undef OPTSTR_i2cget
#define OPTSTR_i2cget "<3>3fy"
#ifdef CLEANUP_i2cget
#undef CLEANUP_i2cget
#undef FOR_i2cget
#undef FLAG_y
#undef FLAG_f
#endif

// i2cset   <4fy
#undef OPTSTR_i2cset
#define OPTSTR_i2cset "<4fy"
#ifdef CLEANUP_i2cset
#undef CLEANUP_i2cset
#undef FOR_i2cset
#undef FLAG_y
#undef FLAG_f
#endif

// iconv   cst:f:
#undef OPTSTR_iconv
#define OPTSTR_iconv "cst:f:"
#ifdef CLEANUP_iconv
#undef CLEANUP_iconv
#undef FOR_iconv
#undef FLAG_f
#undef FLAG_t
#undef FLAG_s
#undef FLAG_c
#endif

// id >1nGgru[!Ggu] >1ZnGgru[!ZGgu]
#undef OPTSTR_id
#define OPTSTR_id ">1nGgru[!Ggu]"
#ifdef CLEANUP_id
#undef CLEANUP_id
#undef FOR_id
#undef FLAG_u
#undef FLAG_r
#undef FLAG_g
#undef FLAG_G
#undef FLAG_n
#undef FLAG_Z
#endif

// ifconfig   ^?aS
#undef OPTSTR_ifconfig
#define OPTSTR_ifconfig "^?aS"
#ifdef CLEANUP_ifconfig
#undef CLEANUP_ifconfig
#undef FOR_ifconfig
#undef FLAG_S
#undef FLAG_a
#endif

// init    
#undef OPTSTR_init
#define OPTSTR_init 0
#ifdef CLEANUP_init
#undef CLEANUP_init
#undef FOR_init
#endif

// inotifyd   <2
#undef OPTSTR_inotifyd
#define OPTSTR_inotifyd "<2"
#ifdef CLEANUP_inotifyd
#undef CLEANUP_inotifyd
#undef FOR_inotifyd
#endif

// insmod   <1
#undef OPTSTR_insmod
#define OPTSTR_insmod "<1"
#ifdef CLEANUP_insmod
#undef CLEANUP_insmod
#undef FOR_insmod
#endif

// install   <1cdDpsvm:o:g:
#undef OPTSTR_install
#define OPTSTR_install "<1cdDpsvm:o:g:"
#ifdef CLEANUP_install
#undef CLEANUP_install
#undef FOR_install
#undef FLAG_g
#undef FLAG_o
#undef FLAG_m
#undef FLAG_v
#undef FLAG_s
#undef FLAG_p
#undef FLAG_D
#undef FLAG_d
#undef FLAG_c
#endif

// ionice   ^tc#<0>3=2n#<0>7=5p#
#undef OPTSTR_ionice
#define OPTSTR_ionice "^tc#<0>3=2n#<0>7=5p#"
#ifdef CLEANUP_ionice
#undef CLEANUP_ionice
#undef FOR_ionice
#undef FLAG_p
#undef FLAG_n
#undef FLAG_c
#undef FLAG_t
#endif

// iorenice   ?<1>3
#undef OPTSTR_iorenice
#define OPTSTR_iorenice "?<1>3"
#ifdef CLEANUP_iorenice
#undef CLEANUP_iorenice
#undef FOR_iorenice
#endif

// iotop   >0AaKOHk*o*p*u*s#<1=7d%<100=3000m#n#<1bq
#undef OPTSTR_iotop
#define OPTSTR_iotop ">0AaKOHk*o*p*u*s#<1=7d%<100=3000m#n#<1bq"
#ifdef CLEANUP_iotop
#undef CLEANUP_iotop
#undef FOR_iotop
#undef FLAG_q
#undef FLAG_b
#undef FLAG_n
#undef FLAG_m
#undef FLAG_d
#undef FLAG_s
#undef FLAG_u
#undef FLAG_p
#undef FLAG_o
#undef FLAG_k
#undef FLAG_H
#undef FLAG_O
#undef FLAG_K
#undef FLAG_a
#undef FLAG_A
#endif

// ip    
#undef OPTSTR_ip
#define OPTSTR_ip 0
#ifdef CLEANUP_ip
#undef CLEANUP_ip
#undef FOR_ip
#endif

// ipcrm   m*M*s*S*q*Q*
#undef OPTSTR_ipcrm
#define OPTSTR_ipcrm "m*M*s*S*q*Q*"
#ifdef CLEANUP_ipcrm
#undef CLEANUP_ipcrm
#undef FOR_ipcrm
#undef FLAG_Q
#undef FLAG_q
#undef FLAG_S
#undef FLAG_s
#undef FLAG_M
#undef FLAG_m
#endif

// ipcs   acptulsqmi#
#undef OPTSTR_ipcs
#define OPTSTR_ipcs "acptulsqmi#"
#ifdef CLEANUP_ipcs
#undef CLEANUP_ipcs
#undef FOR_ipcs
#undef FLAG_i
#undef FLAG_m
#undef FLAG_q
#undef FLAG_s
#undef FLAG_l
#undef FLAG_u
#undef FLAG_t
#undef FLAG_p
#undef FLAG_c
#undef FLAG_a
#endif

// kill   ?ls: 
#undef OPTSTR_kill
#define OPTSTR_kill "?ls: "
#ifdef CLEANUP_kill
#undef CLEANUP_kill
#undef FOR_kill
#undef FLAG_s
#undef FLAG_l
#endif

// killall   ?s:ilqvw
#undef OPTSTR_killall
#define OPTSTR_killall "?s:ilqvw"
#ifdef CLEANUP_killall
#undef CLEANUP_killall
#undef FOR_killall
#undef FLAG_w
#undef FLAG_v
#undef FLAG_q
#undef FLAG_l
#undef FLAG_i
#undef FLAG_s
#endif

// killall5   ?o*ls: [!lo][!ls]
#undef OPTSTR_killall5
#define OPTSTR_killall5 "?o*ls: [!lo][!ls]"
#ifdef CLEANUP_killall5
#undef CLEANUP_killall5
#undef FOR_killall5
#undef FLAG_s
#undef FLAG_l
#undef FLAG_o
#endif

// klogd   c#<1>8n
#undef OPTSTR_klogd
#define OPTSTR_klogd "c#<1>8n"
#ifdef CLEANUP_klogd
#undef CLEANUP_klogd
#undef FOR_klogd
#undef FLAG_n
#undef FLAG_c
#endif

// last   f:W
#undef OPTSTR_last
#define OPTSTR_last "f:W"
#ifdef CLEANUP_last
#undef CLEANUP_last
#undef FOR_last
#undef FLAG_W
#undef FLAG_f
#endif

// link   <2>2
#undef OPTSTR_link
#define OPTSTR_link "<2>2"
#ifdef CLEANUP_link
#undef CLEANUP_link
#undef FOR_link
#endif

// ln <1rt:Tvnfs <1rt:Tvnfs
#undef OPTSTR_ln
#define OPTSTR_ln "<1rt:Tvnfs"
#ifdef CLEANUP_ln
#undef CLEANUP_ln
#undef FOR_ln
#undef FLAG_s
#undef FLAG_f
#undef FLAG_n
#undef FLAG_v
#undef FLAG_T
#undef FLAG_t
#undef FLAG_r
#endif

// load_policy   <1>1
#undef OPTSTR_load_policy
#define OPTSTR_load_policy "<1>1"
#ifdef CLEANUP_load_policy
#undef CLEANUP_load_policy
#undef FOR_load_policy
#endif

// log   <1p:t:
#undef OPTSTR_log
#define OPTSTR_log "<1p:t:"
#ifdef CLEANUP_log
#undef CLEANUP_log
#undef FOR_log
#undef FLAG_t
#undef FLAG_p
#endif

// logger   st:p:
#undef OPTSTR_logger
#define OPTSTR_logger "st:p:"
#ifdef CLEANUP_logger
#undef CLEANUP_logger
#undef FOR_logger
#undef FLAG_p
#undef FLAG_t
#undef FLAG_s
#endif

// login   >1f:ph:
#undef OPTSTR_login
#define OPTSTR_login ">1f:ph:"
#ifdef CLEANUP_login
#undef CLEANUP_login
#undef FOR_login
#undef FLAG_h
#undef FLAG_p
#undef FLAG_f
#endif

// logname   >0
#undef OPTSTR_logname
#define OPTSTR_logname ">0"
#ifdef CLEANUP_logname
#undef CLEANUP_logname
#undef FOR_logname
#endif

// logwrapper    
#undef OPTSTR_logwrapper
#define OPTSTR_logwrapper 0
#ifdef CLEANUP_logwrapper
#undef CLEANUP_logwrapper
#undef FOR_logwrapper
#endif

// losetup   >2S(sizelimit)#s(show)ro#j:fdcaD[!afj]
#undef OPTSTR_losetup
#define OPTSTR_losetup ">2S(sizelimit)#s(show)ro#j:fdcaD[!afj]"
#ifdef CLEANUP_losetup
#undef CLEANUP_losetup
#undef FOR_losetup
#undef FLAG_D
#undef FLAG_a
#undef FLAG_c
#undef FLAG_d
#undef FLAG_f
#undef FLAG_j
#undef FLAG_o
#undef FLAG_r
#undef FLAG_s
#undef FLAG_S
#endif

// ls (color):;(full-time)(show-control-chars)ZgoACFHLRSabcdfhikl@mnpqrstuw#=80<0x1[-Cxm1][-Cxml][-Cxmo][-Cxmg][-cu][-ftS][-HL][!qb] (color):;(full-time)(show-control-chars)ZgoACFHLRSabcdfhikl@mnpqrstuw#=80<0x1[-Cxm1][-Cxml][-Cxmo][-Cxmg][-cu][-ftS][-HL][!qb]
#undef OPTSTR_ls
#define OPTSTR_ls "(color):;(full-time)(show-control-chars)ZgoACFHLRSabcdfhikl@mnpqrstuw#=80<0x1[-Cxm1][-Cxml][-Cxmo][-Cxmg][-cu][-ftS][-HL][!qb]"
#ifdef CLEANUP_ls
#undef CLEANUP_ls
#undef FOR_ls
#undef FLAG_1
#undef FLAG_x
#undef FLAG_w
#undef FLAG_u
#undef FLAG_t
#undef FLAG_s
#undef FLAG_r
#undef FLAG_q
#undef FLAG_p
#undef FLAG_n
#undef FLAG_m
#undef FLAG_l
#undef FLAG_k
#undef FLAG_i
#undef FLAG_h
#undef FLAG_f
#undef FLAG_d
#undef FLAG_c
#undef FLAG_b
#undef FLAG_a
#undef FLAG_S
#undef FLAG_R
#undef FLAG_L
#undef FLAG_H
#undef FLAG_F
#undef FLAG_C
#undef FLAG_A
#undef FLAG_o
#undef FLAG_g
#undef FLAG_Z
#undef FLAG_show_control_chars
#undef FLAG_full_time
#undef FLAG_color
#endif

// lsattr   ldapvR
#undef OPTSTR_lsattr
#define OPTSTR_lsattr "ldapvR"
#ifdef CLEANUP_lsattr
#undef CLEANUP_lsattr
#undef FOR_lsattr
#undef FLAG_R
#undef FLAG_v
#undef FLAG_p
#undef FLAG_a
#undef FLAG_d
#undef FLAG_l
#endif

// lsmod    
#undef OPTSTR_lsmod
#define OPTSTR_lsmod 0
#ifdef CLEANUP_lsmod
#undef CLEANUP_lsmod
#undef FOR_lsmod
#endif

// lsof   lp*t
#undef OPTSTR_lsof
#define OPTSTR_lsof "lp*t"
#ifdef CLEANUP_lsof
#undef CLEANUP_lsof
#undef FOR_lsof
#undef FLAG_t
#undef FLAG_p
#undef FLAG_l
#endif

// lspci   emkn@i:
#undef OPTSTR_lspci
#define OPTSTR_lspci "emkn@i:"
#ifdef CLEANUP_lspci
#undef CLEANUP_lspci
#undef FOR_lspci
#undef FLAG_i
#undef FLAG_n
#undef FLAG_k
#undef FLAG_m
#undef FLAG_e
#endif

// lsusb    
#undef OPTSTR_lsusb
#define OPTSTR_lsusb 0
#ifdef CLEANUP_lsusb
#undef CLEANUP_lsusb
#undef FOR_lsusb
#endif

// makedevs   <1>1d:
#undef OPTSTR_makedevs
#define OPTSTR_makedevs "<1>1d:"
#ifdef CLEANUP_makedevs
#undef CLEANUP_makedevs
#undef FOR_makedevs
#undef FLAG_d
#endif

// man   k:M:
#undef OPTSTR_man
#define OPTSTR_man "k:M:"
#ifdef CLEANUP_man
#undef CLEANUP_man
#undef FOR_man
#undef FLAG_M
#undef FLAG_k
#endif

// mcookie   v(verbose)V(version)
#undef OPTSTR_mcookie
#define OPTSTR_mcookie "v(verbose)V(version)"
#ifdef CLEANUP_mcookie
#undef CLEANUP_mcookie
#undef FOR_mcookie
#undef FLAG_V
#undef FLAG_v
#endif

// md5sum bc(check)s(status)[!bc] bc(check)s(status)[!bc]
#undef OPTSTR_md5sum
#define OPTSTR_md5sum "bc(check)s(status)[!bc]"
#ifdef CLEANUP_md5sum
#undef CLEANUP_md5sum
#undef FOR_md5sum
#undef FLAG_s
#undef FLAG_c
#undef FLAG_b
#endif

// mdev   s
#undef OPTSTR_mdev
#define OPTSTR_mdev "s"
#ifdef CLEANUP_mdev
#undef CLEANUP_mdev
#undef FOR_mdev
#undef FLAG_s
#endif

// microcom <1>1s:X <1>1s:X
#undef OPTSTR_microcom
#define OPTSTR_microcom "<1>1s:X"
#ifdef CLEANUP_microcom
#undef CLEANUP_microcom
#undef FOR_microcom
#undef FLAG_X
#undef FLAG_s
#endif

// mix   c:d:l#r#
#undef OPTSTR_mix
#define OPTSTR_mix "c:d:l#r#"
#ifdef CLEANUP_mix
#undef CLEANUP_mix
#undef FOR_mix
#undef FLAG_r
#undef FLAG_l
#undef FLAG_d
#undef FLAG_c
#endif

// mkdir <1vp(parent)(parents)m: <1Z:vp(parent)(parents)m:
#undef OPTSTR_mkdir
#define OPTSTR_mkdir "<1vp(parent)(parents)m:"
#ifdef CLEANUP_mkdir
#undef CLEANUP_mkdir
#undef FOR_mkdir
#undef FLAG_m
#undef FLAG_p
#undef FLAG_v
#undef FLAG_Z
#endif

// mke2fs   <1>2g:Fnqm#N#i#b#
#undef OPTSTR_mke2fs
#define OPTSTR_mke2fs "<1>2g:Fnqm#N#i#b#"
#ifdef CLEANUP_mke2fs
#undef CLEANUP_mke2fs
#undef FOR_mke2fs
#undef FLAG_b
#undef FLAG_i
#undef FLAG_N
#undef FLAG_m
#undef FLAG_q
#undef FLAG_n
#undef FLAG_F
#undef FLAG_g
#endif

// mkfifo   <1Z:m:
#undef OPTSTR_mkfifo
#define OPTSTR_mkfifo "<1Z:m:"
#ifdef CLEANUP_mkfifo
#undef CLEANUP_mkfifo
#undef FOR_mkfifo
#undef FLAG_m
#undef FLAG_Z
#endif

// mknod   <2>4m(mode):Z:
#undef OPTSTR_mknod
#define OPTSTR_mknod "<2>4m(mode):Z:"
#ifdef CLEANUP_mknod
#undef CLEANUP_mknod
#undef FOR_mknod
#undef FLAG_Z
#undef FLAG_m
#endif

// mkpasswd   >2S:m:P#=0<0
#undef OPTSTR_mkpasswd
#define OPTSTR_mkpasswd ">2S:m:P#=0<0"
#ifdef CLEANUP_mkpasswd
#undef CLEANUP_mkpasswd
#undef FOR_mkpasswd
#undef FLAG_P
#undef FLAG_m
#undef FLAG_S
#endif

// mkswap   <1>1L:
#undef OPTSTR_mkswap
#define OPTSTR_mkswap "<1>1L:"
#ifdef CLEANUP_mkswap
#undef CLEANUP_mkswap
#undef FOR_mkswap
#undef FLAG_L
#endif

// mktemp >1(tmpdir);:uqd(directory)p:t >1(tmpdir);:uqd(directory)p:t
#undef OPTSTR_mktemp
#define OPTSTR_mktemp ">1(tmpdir);:uqd(directory)p:t"
#ifdef CLEANUP_mktemp
#undef CLEANUP_mktemp
#undef FOR_mktemp
#undef FLAG_t
#undef FLAG_p
#undef FLAG_d
#undef FLAG_q
#undef FLAG_u
#undef FLAG_tmpdir
#endif

// modinfo   <1b:k:F:0
#undef OPTSTR_modinfo
#define OPTSTR_modinfo "<1b:k:F:0"
#ifdef CLEANUP_modinfo
#undef CLEANUP_modinfo
#undef FOR_modinfo
#undef FLAG_0
#undef FLAG_F
#undef FLAG_k
#undef FLAG_b
#endif

// modprobe   alrqvsDbd*
#undef OPTSTR_modprobe
#define OPTSTR_modprobe "alrqvsDbd*"
#ifdef CLEANUP_modprobe
#undef CLEANUP_modprobe
#undef FOR_modprobe
#undef FLAG_d
#undef FLAG_b
#undef FLAG_D
#undef FLAG_s
#undef FLAG_v
#undef FLAG_q
#undef FLAG_r
#undef FLAG_l
#undef FLAG_a
#endif

// more    
#undef OPTSTR_more
#define OPTSTR_more 0
#ifdef CLEANUP_more
#undef CLEANUP_more
#undef FOR_more
#endif

// mount   ?O:afnrvwt:o*[-rw]
#undef OPTSTR_mount
#define OPTSTR_mount "?O:afnrvwt:o*[-rw]"
#ifdef CLEANUP_mount
#undef CLEANUP_mount
#undef FOR_mount
#undef FLAG_o
#undef FLAG_t
#undef FLAG_w
#undef FLAG_v
#undef FLAG_r
#undef FLAG_n
#undef FLAG_f
#undef FLAG_a
#undef FLAG_O
#endif

// mountpoint   <1qdx[-dx]
#undef OPTSTR_mountpoint
#define OPTSTR_mountpoint "<1qdx[-dx]"
#ifdef CLEANUP_mountpoint
#undef CLEANUP_mountpoint
#undef FOR_mountpoint
#undef FLAG_x
#undef FLAG_d
#undef FLAG_q
#endif

// mv <2vnF(remove-destination)fiT[-ni] <2vnF(remove-destination)fiT[-ni]
#undef OPTSTR_mv
#define OPTSTR_mv "<2vnF(remove-destination)fiT[-ni]"
#ifdef CLEANUP_mv
#undef CLEANUP_mv
#undef FOR_mv
#undef FLAG_T
#undef FLAG_i
#undef FLAG_f
#undef FLAG_F
#undef FLAG_n
#undef FLAG_v
#endif

// nbd_client   <3>3ns
#undef OPTSTR_nbd_client
#define OPTSTR_nbd_client "<3>3ns"
#ifdef CLEANUP_nbd_client
#undef CLEANUP_nbd_client
#undef FOR_nbd_client
#undef FLAG_s
#undef FLAG_n
#endif

// netcat   ^tlLw#<1W#<1p#<1>65535q#<1s:f:46uU[!tlL][!Lw][!46U]
#undef OPTSTR_netcat
#define OPTSTR_netcat "^tlLw#<1W#<1p#<1>65535q#<1s:f:46uU[!tlL][!Lw][!46U]"
#ifdef CLEANUP_netcat
#undef CLEANUP_netcat
#undef FOR_netcat
#undef FLAG_U
#undef FLAG_u
#undef FLAG_6
#undef FLAG_4
#undef FLAG_f
#undef FLAG_s
#undef FLAG_q
#undef FLAG_p
#undef FLAG_W
#undef FLAG_w
#undef FLAG_L
#undef FLAG_l
#undef FLAG_t
#endif

// netstat   pWrxwutneal
#undef OPTSTR_netstat
#define OPTSTR_netstat "pWrxwutneal"
#ifdef CLEANUP_netstat
#undef CLEANUP_netstat
#undef FOR_netstat
#undef FLAG_l
#undef FLAG_a
#undef FLAG_e
#undef FLAG_n
#undef FLAG_t
#undef FLAG_u
#undef FLAG_w
#undef FLAG_x
#undef FLAG_r
#undef FLAG_W
#undef FLAG_p
#endif

// nice   ^<1n#
#undef OPTSTR_nice
#define OPTSTR_nice "^<1n#"
#ifdef CLEANUP_nice
#undef CLEANUP_nice
#undef FOR_nice
#undef FLAG_n
#endif

// nl   v#=1l#w#<0=6Eb:n:s:
#undef OPTSTR_nl
#define OPTSTR_nl "v#=1l#w#<0=6Eb:n:s:"
#ifdef CLEANUP_nl
#undef CLEANUP_nl
#undef FOR_nl
#undef FLAG_s
#undef FLAG_n
#undef FLAG_b
#undef FLAG_E
#undef FLAG_w
#undef FLAG_l
#undef FLAG_v
#endif

// nohup   <1^
#undef OPTSTR_nohup
#define OPTSTR_nohup "<1^"
#ifdef CLEANUP_nohup
#undef CLEANUP_nohup
#undef FOR_nohup
#endif

// nproc   (all)
#undef OPTSTR_nproc
#define OPTSTR_nproc "(all)"
#ifdef CLEANUP_nproc
#undef CLEANUP_nproc
#undef FOR_nproc
#undef FLAG_all
#endif

// nsenter   <1F(no-fork)t#<1(target)i:(ipc);m:(mount);n:(net);p:(pid);u:(uts);U:(user);
#undef OPTSTR_nsenter
#define OPTSTR_nsenter "<1F(no-fork)t#<1(target)i:(ipc);m:(mount);n:(net);p:(pid);u:(uts);U:(user);"
#ifdef CLEANUP_nsenter
#undef CLEANUP_nsenter
#undef FOR_nsenter
#undef FLAG_U
#undef FLAG_u
#undef FLAG_p
#undef FLAG_n
#undef FLAG_m
#undef FLAG_i
#undef FLAG_t
#undef FLAG_F
#endif

// od j#vw#<1=16N#xsodcbA:t* j#vw#<1=16N#xsodcbA:t*
#undef OPTSTR_od
#define OPTSTR_od "j#vw#<1=16N#xsodcbA:t*"
#ifdef CLEANUP_od
#undef CLEANUP_od
#undef FOR_od
#undef FLAG_t
#undef FLAG_A
#undef FLAG_b
#undef FLAG_c
#undef FLAG_d
#undef FLAG_o
#undef FLAG_s
#undef FLAG_x
#undef FLAG_N
#undef FLAG_w
#undef FLAG_v
#undef FLAG_j
#endif

// oneit   ^<1nc:p3[!pn]
#undef OPTSTR_oneit
#define OPTSTR_oneit "^<1nc:p3[!pn]"
#ifdef CLEANUP_oneit
#undef CLEANUP_oneit
#undef FOR_oneit
#undef FLAG_3
#undef FLAG_p
#undef FLAG_c
#undef FLAG_n
#endif

// openvt   c#<1>63sw
#undef OPTSTR_openvt
#define OPTSTR_openvt "c#<1>63sw"
#ifdef CLEANUP_openvt
#undef CLEANUP_openvt
#undef FOR_openvt
#undef FLAG_w
#undef FLAG_s
#undef FLAG_c
#endif

// partprobe   <1
#undef OPTSTR_partprobe
#define OPTSTR_partprobe "<1"
#ifdef CLEANUP_partprobe
#undef CLEANUP_partprobe
#undef FOR_partprobe
#endif

// passwd   >1a:dlu
#undef OPTSTR_passwd
#define OPTSTR_passwd ">1a:dlu"
#ifdef CLEANUP_passwd
#undef CLEANUP_passwd
#undef FOR_passwd
#undef FLAG_u
#undef FLAG_l
#undef FLAG_d
#undef FLAG_a
#endif

// paste d:s d:s
#undef OPTSTR_paste
#define OPTSTR_paste "d:s"
#ifdef CLEANUP_paste
#undef CLEANUP_paste
#undef FOR_paste
#undef FLAG_s
#undef FLAG_d
#endif

// patch >2(no-backup-if-mismatch)(dry-run)F#g#fulp#d:i:Rs(quiet) >2(no-backup-if-mismatch)(dry-run)xF#g#fulp#d:i:Rs(quiet)
#undef OPTSTR_patch
#define OPTSTR_patch ">2(no-backup-if-mismatch)(dry-run)F#g#fulp#d:i:Rs(quiet)"
#ifdef CLEANUP_patch
#undef CLEANUP_patch
#undef FOR_patch
#undef FLAG_s
#undef FLAG_R
#undef FLAG_i
#undef FLAG_d
#undef FLAG_p
#undef FLAG_l
#undef FLAG_u
#undef FLAG_f
#undef FLAG_g
#undef FLAG_F
#undef FLAG_x
#undef FLAG_dry_run
#undef FLAG_no_backup_if_mismatch
#endif

// pgrep   ?cld:u*U*t*s*P*g*G*fnovxL:[-no]
#undef OPTSTR_pgrep
#define OPTSTR_pgrep "?cld:u*U*t*s*P*g*G*fnovxL:[-no]"
#ifdef CLEANUP_pgrep
#undef CLEANUP_pgrep
#undef FOR_pgrep
#undef FLAG_L
#undef FLAG_x
#undef FLAG_v
#undef FLAG_o
#undef FLAG_n
#undef FLAG_f
#undef FLAG_G
#undef FLAG_g
#undef FLAG_P
#undef FLAG_s
#undef FLAG_t
#undef FLAG_U
#undef FLAG_u
#undef FLAG_d
#undef FLAG_l
#undef FLAG_c
#endif

// pidof   <1so:x
#undef OPTSTR_pidof
#define OPTSTR_pidof "<1so:x"
#ifdef CLEANUP_pidof
#undef CLEANUP_pidof
#undef FOR_pidof
#undef FLAG_x
#undef FLAG_o
#undef FLAG_s
#endif

// ping   <1>1m#t#<0>255=64c#<0=3s#<0>4088=56i%W#<0=3w#<0qf46I:[-46]
#undef OPTSTR_ping
#define OPTSTR_ping "<1>1m#t#<0>255=64c#<0=3s#<0>4088=56i%W#<0=3w#<0qf46I:[-46]"
#ifdef CLEANUP_ping
#undef CLEANUP_ping
#undef FOR_ping
#undef FLAG_I
#undef FLAG_6
#undef FLAG_4
#undef FLAG_f
#undef FLAG_q
#undef FLAG_w
#undef FLAG_W
#undef FLAG_i
#undef FLAG_s
#undef FLAG_c
#undef FLAG_t
#undef FLAG_m
#endif

// pivot_root   <2>2
#undef OPTSTR_pivot_root
#define OPTSTR_pivot_root "<2>2"
#ifdef CLEANUP_pivot_root
#undef CLEANUP_pivot_root
#undef FOR_pivot_root
#endif

// pkill   ?Vu*U*t*s*P*g*G*fnovxl:[-no]
#undef OPTSTR_pkill
#define OPTSTR_pkill "?Vu*U*t*s*P*g*G*fnovxl:[-no]"
#ifdef CLEANUP_pkill
#undef CLEANUP_pkill
#undef FOR_pkill
#undef FLAG_l
#undef FLAG_x
#undef FLAG_v
#undef FLAG_o
#undef FLAG_n
#undef FLAG_f
#undef FLAG_G
#undef FLAG_g
#undef FLAG_P
#undef FLAG_s
#undef FLAG_t
#undef FLAG_U
#undef FLAG_u
#undef FLAG_V
#endif

// pmap   <1xq
#undef OPTSTR_pmap
#define OPTSTR_pmap "<1xq"
#ifdef CLEANUP_pmap
#undef CLEANUP_pmap
#undef FOR_pmap
#undef FLAG_q
#undef FLAG_x
#endif

// printenv   0(null)
#undef OPTSTR_printenv
#define OPTSTR_printenv "0(null)"
#ifdef CLEANUP_printenv
#undef CLEANUP_printenv
#undef FOR_printenv
#undef FLAG_0
#endif

// printf   <1?^
#undef OPTSTR_printf
#define OPTSTR_printf "<1?^"
#ifdef CLEANUP_printf
#undef CLEANUP_printf
#undef FOR_printf
#endif

// ps   k(sort)*P(ppid)*aAdeflMno*O*p(pid)*s*t*Tu*U*g*G*wZ[!ol][+Ae][!oO]
#undef OPTSTR_ps
#define OPTSTR_ps "k(sort)*P(ppid)*aAdeflMno*O*p(pid)*s*t*Tu*U*g*G*wZ[!ol][+Ae][!oO]"
#ifdef CLEANUP_ps
#undef CLEANUP_ps
#undef FOR_ps
#undef FLAG_Z
#undef FLAG_w
#undef FLAG_G
#undef FLAG_g
#undef FLAG_U
#undef FLAG_u
#undef FLAG_T
#undef FLAG_t
#undef FLAG_s
#undef FLAG_p
#undef FLAG_O
#undef FLAG_o
#undef FLAG_n
#undef FLAG_M
#undef FLAG_l
#undef FLAG_f
#undef FLAG_e
#undef FLAG_d
#undef FLAG_A
#undef FLAG_a
#undef FLAG_P
#undef FLAG_k
#endif

// pwd >0LP[-LP] >0LP[-LP]
#undef OPTSTR_pwd
#define OPTSTR_pwd ">0LP[-LP]"
#ifdef CLEANUP_pwd
#undef CLEANUP_pwd
#undef FOR_pwd
#undef FLAG_P
#undef FLAG_L
#endif

// pwdx   <1a
#undef OPTSTR_pwdx
#define OPTSTR_pwdx "<1a"
#ifdef CLEANUP_pwdx
#undef CLEANUP_pwdx
#undef FOR_pwdx
#undef FLAG_a
#endif

// readahead    
#undef OPTSTR_readahead
#define OPTSTR_readahead 0
#ifdef CLEANUP_readahead
#undef CLEANUP_readahead
#undef FOR_readahead
#endif

// readelf   <1(dyn-syms)adhlnp:SsWx:
#undef OPTSTR_readelf
#define OPTSTR_readelf "<1(dyn-syms)adhlnp:SsWx:"
#ifdef CLEANUP_readelf
#undef CLEANUP_readelf
#undef FOR_readelf
#undef FLAG_x
#undef FLAG_W
#undef FLAG_s
#undef FLAG_S
#undef FLAG_p
#undef FLAG_n
#undef FLAG_l
#undef FLAG_h
#undef FLAG_d
#undef FLAG_a
#undef FLAG_dyn_syms
#endif

// readlink <1nqmef(canonicalize)[-mef] <1nqmef(canonicalize)[-mef]
#undef OPTSTR_readlink
#define OPTSTR_readlink "<1nqmef(canonicalize)[-mef]"
#ifdef CLEANUP_readlink
#undef CLEANUP_readlink
#undef FOR_readlink
#undef FLAG_f
#undef FLAG_e
#undef FLAG_m
#undef FLAG_q
#undef FLAG_n
#endif

// realpath <1 <1
#undef OPTSTR_realpath
#define OPTSTR_realpath "<1"
#ifdef CLEANUP_realpath
#undef CLEANUP_realpath
#undef FOR_realpath
#endif

// reboot   fn
#undef OPTSTR_reboot
#define OPTSTR_reboot "fn"
#ifdef CLEANUP_reboot
#undef CLEANUP_reboot
#undef FOR_reboot
#undef FLAG_n
#undef FLAG_f
#endif

// renice   <1gpun#|
#undef OPTSTR_renice
#define OPTSTR_renice "<1gpun#|"
#ifdef CLEANUP_renice
#undef CLEANUP_renice
#undef FOR_renice
#undef FLAG_n
#undef FLAG_u
#undef FLAG_p
#undef FLAG_g
#endif

// reset    
#undef OPTSTR_reset
#define OPTSTR_reset 0
#ifdef CLEANUP_reset
#undef CLEANUP_reset
#undef FOR_reset
#endif

// restorecon   <1DFnRrv
#undef OPTSTR_restorecon
#define OPTSTR_restorecon "<1DFnRrv"
#ifdef CLEANUP_restorecon
#undef CLEANUP_restorecon
#undef FOR_restorecon
#undef FLAG_v
#undef FLAG_r
#undef FLAG_R
#undef FLAG_n
#undef FLAG_F
#undef FLAG_D
#endif

// rev    
#undef OPTSTR_rev
#define OPTSTR_rev 0
#ifdef CLEANUP_rev
#undef CLEANUP_rev
#undef FOR_rev
#endif

// rfkill   <1>2
#undef OPTSTR_rfkill
#define OPTSTR_rfkill "<1>2"
#ifdef CLEANUP_rfkill
#undef CLEANUP_rfkill
#undef FOR_rfkill
#endif

// rm fiRrv[-fi] fiRrv[-fi]
#undef OPTSTR_rm
#define OPTSTR_rm "fiRrv[-fi]"
#ifdef CLEANUP_rm
#undef CLEANUP_rm
#undef FOR_rm
#undef FLAG_v
#undef FLAG_r
#undef FLAG_R
#undef FLAG_i
#undef FLAG_f
#endif

// rmdir <1(ignore-fail-on-non-empty)p <1(ignore-fail-on-non-empty)p
#undef OPTSTR_rmdir
#define OPTSTR_rmdir "<1(ignore-fail-on-non-empty)p"
#ifdef CLEANUP_rmdir
#undef CLEANUP_rmdir
#undef FOR_rmdir
#undef FLAG_p
#undef FLAG_ignore_fail_on_non_empty
#endif

// rmmod   <1wf
#undef OPTSTR_rmmod
#define OPTSTR_rmmod "<1wf"
#ifdef CLEANUP_rmmod
#undef CLEANUP_rmmod
#undef FOR_rmmod
#undef FLAG_f
#undef FLAG_w
#endif

// route   ?neA:
#undef OPTSTR_route
#define OPTSTR_route "?neA:"
#ifdef CLEANUP_route
#undef CLEANUP_route
#undef FOR_route
#undef FLAG_A
#undef FLAG_e
#undef FLAG_n
#endif

// runcon   <2
#undef OPTSTR_runcon
#define OPTSTR_runcon "<2"
#ifdef CLEANUP_runcon
#undef CLEANUP_runcon
#undef FOR_runcon
#endif

// sed (help)(version)e*f*i:;nErz(null-data)[+Er] (help)(version)e*f*i:;nErz(null-data)[+Er]
#undef OPTSTR_sed
#define OPTSTR_sed "(help)(version)e*f*i:;nErz(null-data)[+Er]"
#ifdef CLEANUP_sed
#undef CLEANUP_sed
#undef FOR_sed
#undef FLAG_z
#undef FLAG_r
#undef FLAG_E
#undef FLAG_n
#undef FLAG_i
#undef FLAG_f
#undef FLAG_e
#undef FLAG_version
#undef FLAG_help
#endif

// sendevent   <4>4
#undef OPTSTR_sendevent
#define OPTSTR_sendevent "<4>4"
#ifdef CLEANUP_sendevent
#undef CLEANUP_sendevent
#undef FOR_sendevent
#endif

// seq <1>3?f:s:w[!fw] <1>3?f:s:w[!fw]
#undef OPTSTR_seq
#define OPTSTR_seq "<1>3?f:s:w[!fw]"
#ifdef CLEANUP_seq
#undef CLEANUP_seq
#undef FOR_seq
#undef FLAG_w
#undef FLAG_s
#undef FLAG_f
#endif

// setenforce   <1>1
#undef OPTSTR_setenforce
#define OPTSTR_setenforce "<1>1"
#ifdef CLEANUP_setenforce
#undef CLEANUP_setenforce
#undef FOR_setenforce
#endif

// setfattr   hn:|v:x:|[!xv]
#undef OPTSTR_setfattr
#define OPTSTR_setfattr "hn:|v:x:|[!xv]"
#ifdef CLEANUP_setfattr
#undef CLEANUP_setfattr
#undef FOR_setfattr
#undef FLAG_x
#undef FLAG_v
#undef FLAG_n
#undef FLAG_h
#endif

// setsid ^<1wcd[!dc] ^<1wcd[!dc]
#undef OPTSTR_setsid
#define OPTSTR_setsid "^<1wcd[!dc]"
#ifdef CLEANUP_setsid
#undef CLEANUP_setsid
#undef FOR_setsid
#undef FLAG_d
#undef FLAG_c
#undef FLAG_w
#endif

// sh   (noediting)(noprofile)(norc)sc:i
#undef OPTSTR_sh
#define OPTSTR_sh "(noediting)(noprofile)(norc)sc:i"
#ifdef CLEANUP_sh
#undef CLEANUP_sh
#undef FOR_sh
#undef FLAG_i
#undef FLAG_c
#undef FLAG_s
#undef FLAG_norc
#undef FLAG_noprofile
#undef FLAG_noediting
#endif

// sha1sum bc(check)s(status)[!bc] bc(check)s(status)[!bc]
#undef OPTSTR_sha1sum
#define OPTSTR_sha1sum "bc(check)s(status)[!bc]"
#ifdef CLEANUP_sha1sum
#undef CLEANUP_sha1sum
#undef FOR_sha1sum
#undef FLAG_s
#undef FLAG_c
#undef FLAG_b
#endif

// shred   <1zxus#<1n#<1o#<0f
#undef OPTSTR_shred
#define OPTSTR_shred "<1zxus#<1n#<1o#<0f"
#ifdef CLEANUP_shred
#undef CLEANUP_shred
#undef FOR_shred
#undef FLAG_f
#undef FLAG_o
#undef FLAG_n
#undef FLAG_s
#undef FLAG_u
#undef FLAG_x
#undef FLAG_z
#endif

// skeleton   (walrus)(blubber):;(also):e@d*c#b:a
#undef OPTSTR_skeleton
#define OPTSTR_skeleton "(walrus)(blubber):;(also):e@d*c#b:a"
#ifdef CLEANUP_skeleton
#undef CLEANUP_skeleton
#undef FOR_skeleton
#undef FLAG_a
#undef FLAG_b
#undef FLAG_c
#undef FLAG_d
#undef FLAG_e
#undef FLAG_also
#undef FLAG_blubber
#undef FLAG_walrus
#endif

// skeleton_alias   b#dq
#undef OPTSTR_skeleton_alias
#define OPTSTR_skeleton_alias "b#dq"
#ifdef CLEANUP_skeleton_alias
#undef CLEANUP_skeleton_alias
#undef FOR_skeleton_alias
#undef FLAG_q
#undef FLAG_d
#undef FLAG_b
#endif

// sleep <1 <1
#undef OPTSTR_sleep
#define OPTSTR_sleep "<1"
#ifdef CLEANUP_sleep
#undef CLEANUP_sleep
#undef FOR_sleep
#endif

// sntp   >1M :m :Sp:t#<0=1>16asdDqr#<4>17=10[!as]
#undef OPTSTR_sntp
#define OPTSTR_sntp ">1M :m :Sp:t#<0=1>16asdDqr#<4>17=10[!as]"
#ifdef CLEANUP_sntp
#undef CLEANUP_sntp
#undef FOR_sntp
#undef FLAG_r
#undef FLAG_q
#undef FLAG_D
#undef FLAG_d
#undef FLAG_s
#undef FLAG_a
#undef FLAG_t
#undef FLAG_p
#undef FLAG_S
#undef FLAG_m
#undef FLAG_M
#endif

// sort gS:T:mo:k*t:xVbMcszdfirun gS:T:mo:k*t:xVbMcszdfirun
#undef OPTSTR_sort
#define OPTSTR_sort "gS:T:mo:k*t:xVbMcszdfirun"
#ifdef CLEANUP_sort
#undef CLEANUP_sort
#undef FOR_sort
#undef FLAG_n
#undef FLAG_u
#undef FLAG_r
#undef FLAG_i
#undef FLAG_f
#undef FLAG_d
#undef FLAG_z
#undef FLAG_s
#undef FLAG_c
#undef FLAG_M
#undef FLAG_b
#undef FLAG_V
#undef FLAG_x
#undef FLAG_t
#undef FLAG_k
#undef FLAG_o
#undef FLAG_m
#undef FLAG_T
#undef FLAG_S
#undef FLAG_g
#endif

// split   >2a#<1=2>9b#<1l#<1[!bl]
#undef OPTSTR_split
#define OPTSTR_split ">2a#<1=2>9b#<1l#<1[!bl]"
#ifdef CLEANUP_split
#undef CLEANUP_split
#undef FOR_split
#undef FLAG_l
#undef FLAG_b
#undef FLAG_a
#endif

// stat <1c:(format)fLt <1c:(format)fLt
#undef OPTSTR_stat
#define OPTSTR_stat "<1c:(format)fLt"
#ifdef CLEANUP_stat
#undef CLEANUP_stat
#undef FOR_stat
#undef FLAG_t
#undef FLAG_L
#undef FLAG_f
#undef FLAG_c
#endif

// strings   t:an#=4<1fo
#undef OPTSTR_strings
#define OPTSTR_strings "t:an#=4<1fo"
#ifdef CLEANUP_strings
#undef CLEANUP_strings
#undef FOR_strings
#undef FLAG_o
#undef FLAG_f
#undef FLAG_n
#undef FLAG_a
#undef FLAG_t
#endif

// stty   ?aF:g[!ag]
#undef OPTSTR_stty
#define OPTSTR_stty "?aF:g[!ag]"
#ifdef CLEANUP_stty
#undef CLEANUP_stty
#undef FOR_stty
#undef FLAG_g
#undef FLAG_F
#undef FLAG_a
#endif

// su   ^lmpu:g:c:s:[!lmp]
#undef OPTSTR_su
#define OPTSTR_su "^lmpu:g:c:s:[!lmp]"
#ifdef CLEANUP_su
#undef CLEANUP_su
#undef FOR_su
#undef FLAG_s
#undef FLAG_c
#undef FLAG_g
#undef FLAG_u
#undef FLAG_p
#undef FLAG_m
#undef FLAG_l
#endif

// sulogin   t#<0=0
#undef OPTSTR_sulogin
#define OPTSTR_sulogin "t#<0=0"
#ifdef CLEANUP_sulogin
#undef CLEANUP_sulogin
#undef FOR_sulogin
#undef FLAG_t
#endif

// swapoff   <1>1
#undef OPTSTR_swapoff
#define OPTSTR_swapoff "<1>1"
#ifdef CLEANUP_swapoff
#undef CLEANUP_swapoff
#undef FOR_swapoff
#endif

// swapon   <1>1p#<0>32767d
#undef OPTSTR_swapon
#define OPTSTR_swapon "<1>1p#<0>32767d"
#ifdef CLEANUP_swapon
#undef CLEANUP_swapon
#undef FOR_swapon
#undef FLAG_d
#undef FLAG_p
#endif

// switch_root   <2c:h
#undef OPTSTR_switch_root
#define OPTSTR_switch_root "<2c:h"
#ifdef CLEANUP_switch_root
#undef CLEANUP_switch_root
#undef FOR_switch_root
#undef FLAG_h
#undef FLAG_c
#endif

// sync    
#undef OPTSTR_sync
#define OPTSTR_sync 0
#ifdef CLEANUP_sync
#undef CLEANUP_sync
#undef FOR_sync
#endif

// sysctl   ^neNqwpaA[!ap][!aq][!aw][+aA]
#undef OPTSTR_sysctl
#define OPTSTR_sysctl "^neNqwpaA[!ap][!aq][!aw][+aA]"
#ifdef CLEANUP_sysctl
#undef CLEANUP_sysctl
#undef FOR_sysctl
#undef FLAG_A
#undef FLAG_a
#undef FLAG_p
#undef FLAG_w
#undef FLAG_q
#undef FLAG_N
#undef FLAG_e
#undef FLAG_n
#endif

// syslogd   >0l#<1>8=8R:b#<0>99=1s#<0=200m#<0>71582787=20O:p:f:a:nSKLD
#undef OPTSTR_syslogd
#define OPTSTR_syslogd ">0l#<1>8=8R:b#<0>99=1s#<0=200m#<0>71582787=20O:p:f:a:nSKLD"
#ifdef CLEANUP_syslogd
#undef CLEANUP_syslogd
#undef FOR_syslogd
#undef FLAG_D
#undef FLAG_L
#undef FLAG_K
#undef FLAG_S
#undef FLAG_n
#undef FLAG_a
#undef FLAG_f
#undef FLAG_p
#undef FLAG_O
#undef FLAG_m
#undef FLAG_s
#undef FLAG_b
#undef FLAG_R
#undef FLAG_l
#endif

// tac    
#undef OPTSTR_tac
#define OPTSTR_tac 0
#ifdef CLEANUP_tac
#undef CLEANUP_tac
#undef FOR_tac
#endif

// tail ?fc-n-[-cn] ?fc-n-[-cn]
#undef OPTSTR_tail
#define OPTSTR_tail "?fc-n-[-cn]"
#ifdef CLEANUP_tail
#undef CLEANUP_tail
#undef FOR_tail
#undef FLAG_n
#undef FLAG_c
#undef FLAG_f
#endif

// tar &(restrict)(full-time)(no-recursion)(numeric-owner)(no-same-permissions)(overwrite)(exclude)*(mode):(mtime):(group):(owner):(to-command):o(no-same-owner)p(same-permissions)k(keep-old)c(create)|h(dereference)x(extract)|t(list)|v(verbose)J(xz)j(bzip2)z(gzip)S(sparse)O(to-stdout)m(touch)X(exclude-from)*T(files-from)*C(directory):f(file):a[!txc][!jzJa] &(restrict)(full-time)(no-recursion)(numeric-owner)(no-same-permissions)(overwrite)(exclude)*(mode):(mtime):(group):(owner):(to-command):o(no-same-owner)p(same-permissions)k(keep-old)c(create)|h(dereference)x(extract)|t(list)|v(verbose)J(xz)j(bzip2)z(gzip)S(sparse)O(to-stdout)m(touch)X(exclude-from)*T(files-from)*C(directory):f(file):a[!txc][!jzJa]
#undef OPTSTR_tar
#define OPTSTR_tar "&(restrict)(full-time)(no-recursion)(numeric-owner)(no-same-permissions)(overwrite)(exclude)*(mode):(mtime):(group):(owner):(to-command):o(no-same-owner)p(same-permissions)k(keep-old)c(create)|h(dereference)x(extract)|t(list)|v(verbose)J(xz)j(bzip2)z(gzip)S(sparse)O(to-stdout)m(touch)X(exclude-from)*T(files-from)*C(directory):f(file):a[!txc][!jzJa]"
#ifdef CLEANUP_tar
#undef CLEANUP_tar
#undef FOR_tar
#undef FLAG_a
#undef FLAG_f
#undef FLAG_C
#undef FLAG_T
#undef FLAG_X
#undef FLAG_m
#undef FLAG_O
#undef FLAG_S
#undef FLAG_z
#undef FLAG_j
#undef FLAG_J
#undef FLAG_v
#undef FLAG_t
#undef FLAG_x
#undef FLAG_h
#undef FLAG_c
#undef FLAG_k
#undef FLAG_p
#undef FLAG_o
#undef FLAG_to_command
#undef FLAG_owner
#undef FLAG_group
#undef FLAG_mtime
#undef FLAG_mode
#undef FLAG_exclude
#undef FLAG_overwrite
#undef FLAG_no_same_permissions
#undef FLAG_numeric_owner
#undef FLAG_no_recursion
#undef FLAG_full_time
#undef FLAG_restrict
#endif

// taskset   <1^pa
#undef OPTSTR_taskset
#define OPTSTR_taskset "<1^pa"
#ifdef CLEANUP_taskset
#undef CLEANUP_taskset
#undef FOR_taskset
#undef FLAG_a
#undef FLAG_p
#endif

// tcpsvd   ^<3c#=30<1C:b#=20<0u:l:hEv
#undef OPTSTR_tcpsvd
#define OPTSTR_tcpsvd "^<3c#=30<1C:b#=20<0u:l:hEv"
#ifdef CLEANUP_tcpsvd
#undef CLEANUP_tcpsvd
#undef FOR_tcpsvd
#undef FLAG_v
#undef FLAG_E
#undef FLAG_h
#undef FLAG_l
#undef FLAG_u
#undef FLAG_b
#undef FLAG_C
#undef FLAG_c
#endif

// tee ia ia
#undef OPTSTR_tee
#define OPTSTR_tee "ia"
#ifdef CLEANUP_tee
#undef CLEANUP_tee
#undef FOR_tee
#undef FLAG_a
#undef FLAG_i
#endif

// telnet   <1>2
#undef OPTSTR_telnet
#define OPTSTR_telnet "<1>2"
#ifdef CLEANUP_telnet
#undef CLEANUP_telnet
#undef FOR_telnet
#endif

// telnetd   w#<0b:p#<0>65535=23f:l:FSKi[!wi]
#undef OPTSTR_telnetd
#define OPTSTR_telnetd "w#<0b:p#<0>65535=23f:l:FSKi[!wi]"
#ifdef CLEANUP_telnetd
#undef CLEANUP_telnetd
#undef FOR_telnetd
#undef FLAG_i
#undef FLAG_K
#undef FLAG_S
#undef FLAG_F
#undef FLAG_l
#undef FLAG_f
#undef FLAG_p
#undef FLAG_b
#undef FLAG_w
#endif

// test    
#undef OPTSTR_test
#define OPTSTR_test 0
#ifdef CLEANUP_test
#undef CLEANUP_test
#undef FOR_test
#endif

// tftp   <1b#<8>65464r:l:g|p|[!gp]
#undef OPTSTR_tftp
#define OPTSTR_tftp "<1b#<8>65464r:l:g|p|[!gp]"
#ifdef CLEANUP_tftp
#undef CLEANUP_tftp
#undef FOR_tftp
#undef FLAG_p
#undef FLAG_g
#undef FLAG_l
#undef FLAG_r
#undef FLAG_b
#endif

// tftpd   rcu:l
#undef OPTSTR_tftpd
#define OPTSTR_tftpd "rcu:l"
#ifdef CLEANUP_tftpd
#undef CLEANUP_tftpd
#undef FOR_tftpd
#undef FLAG_l
#undef FLAG_u
#undef FLAG_c
#undef FLAG_r
#endif

// time   <1^pv
#undef OPTSTR_time
#define OPTSTR_time "<1^pv"
#ifdef CLEANUP_time
#undef CLEANUP_time
#undef FOR_time
#undef FLAG_v
#undef FLAG_p
#endif

// timeout <2^(foreground)(preserve-status)vk:s(signal): <2^(foreground)(preserve-status)vk:s(signal):
#undef OPTSTR_timeout
#define OPTSTR_timeout "<2^(foreground)(preserve-status)vk:s(signal):"
#ifdef CLEANUP_timeout
#undef CLEANUP_timeout
#undef FOR_timeout
#undef FLAG_s
#undef FLAG_k
#undef FLAG_v
#undef FLAG_preserve_status
#undef FLAG_foreground
#endif

// top   >0O*Hk*o*p*u*s#<1d%<100=3000m#n#<1bq[!oO]
#undef OPTSTR_top
#define OPTSTR_top ">0O*Hk*o*p*u*s#<1d%<100=3000m#n#<1bq[!oO]"
#ifdef CLEANUP_top
#undef CLEANUP_top
#undef FOR_top
#undef FLAG_q
#undef FLAG_b
#undef FLAG_n
#undef FLAG_m
#undef FLAG_d
#undef FLAG_s
#undef FLAG_u
#undef FLAG_p
#undef FLAG_o
#undef FLAG_k
#undef FLAG_H
#undef FLAG_O
#endif

// touch <1acd:fmr:t:h[!dtr] <1acd:fmr:t:h[!dtr]
#undef OPTSTR_touch
#define OPTSTR_touch "<1acd:fmr:t:h[!dtr]"
#ifdef CLEANUP_touch
#undef CLEANUP_touch
#undef FOR_touch
#undef FLAG_h
#undef FLAG_t
#undef FLAG_r
#undef FLAG_m
#undef FLAG_f
#undef FLAG_d
#undef FLAG_c
#undef FLAG_a
#endif

// toybox    
#undef OPTSTR_toybox
#define OPTSTR_toybox 0
#ifdef CLEANUP_toybox
#undef CLEANUP_toybox
#undef FOR_toybox
#endif

// tr ^>2<1Ccsd[+cC] ^>2<1Ccsd[+cC]
#undef OPTSTR_tr
#define OPTSTR_tr "^>2<1Ccsd[+cC]"
#ifdef CLEANUP_tr
#undef CLEANUP_tr
#undef FOR_tr
#undef FLAG_d
#undef FLAG_s
#undef FLAG_c
#undef FLAG_C
#endif

// traceroute   <1>2i:f#<1>255=1z#<0>86400=0g*w#<0>86400=5t#<0>255=0s:q#<1>255=3p#<1>65535=33434m#<1>255=30rvndlIUF64
#undef OPTSTR_traceroute
#define OPTSTR_traceroute "<1>2i:f#<1>255=1z#<0>86400=0g*w#<0>86400=5t#<0>255=0s:q#<1>255=3p#<1>65535=33434m#<1>255=30rvndlIUF64"
#ifdef CLEANUP_traceroute
#undef CLEANUP_traceroute
#undef FOR_traceroute
#undef FLAG_4
#undef FLAG_6
#undef FLAG_F
#undef FLAG_U
#undef FLAG_I
#undef FLAG_l
#undef FLAG_d
#undef FLAG_n
#undef FLAG_v
#undef FLAG_r
#undef FLAG_m
#undef FLAG_p
#undef FLAG_q
#undef FLAG_s
#undef FLAG_t
#undef FLAG_w
#undef FLAG_g
#undef FLAG_z
#undef FLAG_f
#undef FLAG_i
#endif

// true    
#undef OPTSTR_true
#define OPTSTR_true 0
#ifdef CLEANUP_true
#undef CLEANUP_true
#undef FOR_true
#endif

// truncate <1s:|c <1s:|c
#undef OPTSTR_truncate
#define OPTSTR_truncate "<1s:|c"
#ifdef CLEANUP_truncate
#undef CLEANUP_truncate
#undef FOR_truncate
#undef FLAG_c
#undef FLAG_s
#endif

// tty   s
#undef OPTSTR_tty
#define OPTSTR_tty "s"
#ifdef CLEANUP_tty
#undef CLEANUP_tty
#undef FOR_tty
#undef FLAG_s
#endif

// tunctl   <1>1t|d|u:T[!td]
#undef OPTSTR_tunctl
#define OPTSTR_tunctl "<1>1t|d|u:T[!td]"
#ifdef CLEANUP_tunctl
#undef CLEANUP_tunctl
#undef FOR_tunctl
#undef FLAG_T
#undef FLAG_u
#undef FLAG_d
#undef FLAG_t
#endif

// ulimit   >1P#<1SHavutsrRqpnmlifedc[-SH][!apvutsrRqnmlifedc]
#undef OPTSTR_ulimit
#define OPTSTR_ulimit ">1P#<1SHavutsrRqpnmlifedc[-SH][!apvutsrRqnmlifedc]"
#ifdef CLEANUP_ulimit
#undef CLEANUP_ulimit
#undef FOR_ulimit
#undef FLAG_c
#undef FLAG_d
#undef FLAG_e
#undef FLAG_f
#undef FLAG_i
#undef FLAG_l
#undef FLAG_m
#undef FLAG_n
#undef FLAG_p
#undef FLAG_q
#undef FLAG_R
#undef FLAG_r
#undef FLAG_s
#undef FLAG_t
#undef FLAG_u
#undef FLAG_v
#undef FLAG_a
#undef FLAG_H
#undef FLAG_S
#undef FLAG_P
#endif

// umount   cndDflrat*v[!na]
#undef OPTSTR_umount
#define OPTSTR_umount "cndDflrat*v[!na]"
#ifdef CLEANUP_umount
#undef CLEANUP_umount
#undef FOR_umount
#undef FLAG_v
#undef FLAG_t
#undef FLAG_a
#undef FLAG_r
#undef FLAG_l
#undef FLAG_f
#undef FLAG_D
#undef FLAG_d
#undef FLAG_n
#undef FLAG_c
#endif

// uname oamvrns[+os] oamvrns[+os]
#undef OPTSTR_uname
#define OPTSTR_uname "oamvrns[+os]"
#ifdef CLEANUP_uname
#undef CLEANUP_uname
#undef FOR_uname
#undef FLAG_s
#undef FLAG_n
#undef FLAG_r
#undef FLAG_v
#undef FLAG_m
#undef FLAG_a
#undef FLAG_o
#endif

// uniq f#s#w#zicdu f#s#w#zicdu
#undef OPTSTR_uniq
#define OPTSTR_uniq "f#s#w#zicdu"
#ifdef CLEANUP_uniq
#undef CLEANUP_uniq
#undef FOR_uniq
#undef FLAG_u
#undef FLAG_d
#undef FLAG_c
#undef FLAG_i
#undef FLAG_z
#undef FLAG_w
#undef FLAG_s
#undef FLAG_f
#endif

// unix2dos    
#undef OPTSTR_unix2dos
#define OPTSTR_unix2dos 0
#ifdef CLEANUP_unix2dos
#undef CLEANUP_unix2dos
#undef FOR_unix2dos
#endif

// unlink   <1>1
#undef OPTSTR_unlink
#define OPTSTR_unlink "<1>1"
#ifdef CLEANUP_unlink
#undef CLEANUP_unlink
#undef FOR_unlink
#endif

// unshare   <1^f(fork);r(map-root-user);i:(ipc);m:(mount);n:(net);p:(pid);u:(uts);U:(user);
#undef OPTSTR_unshare
#define OPTSTR_unshare "<1^f(fork);r(map-root-user);i:(ipc);m:(mount);n:(net);p:(pid);u:(uts);U:(user);"
#ifdef CLEANUP_unshare
#undef CLEANUP_unshare
#undef FOR_unshare
#undef FLAG_U
#undef FLAG_u
#undef FLAG_p
#undef FLAG_n
#undef FLAG_m
#undef FLAG_i
#undef FLAG_r
#undef FLAG_f
#endif

// uptime   >0ps
#undef OPTSTR_uptime
#define OPTSTR_uptime ">0ps"
#ifdef CLEANUP_uptime
#undef CLEANUP_uptime
#undef FOR_uptime
#undef FLAG_s
#undef FLAG_p
#endif

// useradd   <1>2u#<0G:s:g:h:SDH
#undef OPTSTR_useradd
#define OPTSTR_useradd "<1>2u#<0G:s:g:h:SDH"
#ifdef CLEANUP_useradd
#undef CLEANUP_useradd
#undef FOR_useradd
#undef FLAG_H
#undef FLAG_D
#undef FLAG_S
#undef FLAG_h
#undef FLAG_g
#undef FLAG_s
#undef FLAG_G
#undef FLAG_u
#endif

// userdel   <1>1r
#undef OPTSTR_userdel
#define OPTSTR_userdel "<1>1r"
#ifdef CLEANUP_userdel
#undef CLEANUP_userdel
#undef FOR_userdel
#undef FLAG_r
#endif

// usleep   <1
#undef OPTSTR_usleep
#define OPTSTR_usleep "<1"
#ifdef CLEANUP_usleep
#undef CLEANUP_usleep
#undef FOR_usleep
#endif

// uudecode   >1o:
#undef OPTSTR_uudecode
#define OPTSTR_uudecode ">1o:"
#ifdef CLEANUP_uudecode
#undef CLEANUP_uudecode
#undef FOR_uudecode
#undef FLAG_o
#endif

// uuencode   <1>2m
#undef OPTSTR_uuencode
#define OPTSTR_uuencode "<1>2m"
#ifdef CLEANUP_uuencode
#undef CLEANUP_uuencode
#undef FOR_uuencode
#undef FLAG_m
#endif

// uuidgen   >0r(random)
#undef OPTSTR_uuidgen
#define OPTSTR_uuidgen ">0r(random)"
#ifdef CLEANUP_uuidgen
#undef CLEANUP_uuidgen
#undef FOR_uuidgen
#undef FLAG_r
#endif

// vconfig   <2>4
#undef OPTSTR_vconfig
#define OPTSTR_vconfig "<2>4"
#ifdef CLEANUP_vconfig
#undef CLEANUP_vconfig
#undef FOR_vconfig
#endif

// vi   >1s:
#undef OPTSTR_vi
#define OPTSTR_vi ">1s:"
#ifdef CLEANUP_vi
#undef CLEANUP_vi
#undef FOR_vi
#undef FLAG_s
#endif

// vmstat   >2n
#undef OPTSTR_vmstat
#define OPTSTR_vmstat ">2n"
#ifdef CLEANUP_vmstat
#undef CLEANUP_vmstat
#undef FOR_vmstat
#undef FLAG_n
#endif

// w    
#undef OPTSTR_w
#define OPTSTR_w 0
#ifdef CLEANUP_w
#undef CLEANUP_w
#undef FOR_w
#endif

// watch   ^<1n%<100=2000tebx
#undef OPTSTR_watch
#define OPTSTR_watch "^<1n%<100=2000tebx"
#ifdef CLEANUP_watch
#undef CLEANUP_watch
#undef FOR_watch
#undef FLAG_x
#undef FLAG_b
#undef FLAG_e
#undef FLAG_t
#undef FLAG_n
#endif

// wc mcwl mcwl
#undef OPTSTR_wc
#define OPTSTR_wc "mcwl"
#ifdef CLEANUP_wc
#undef CLEANUP_wc
#undef FOR_wc
#undef FLAG_l
#undef FLAG_w
#undef FLAG_c
#undef FLAG_m
#endif

// wget   (no-check-certificate)O:
#undef OPTSTR_wget
#define OPTSTR_wget "(no-check-certificate)O:"
#ifdef CLEANUP_wget
#undef CLEANUP_wget
#undef FOR_wget
#undef FLAG_O
#undef FLAG_no_check_certificate
#endif

// which <1a <1a
#undef OPTSTR_which
#define OPTSTR_which "<1a"
#ifdef CLEANUP_which
#undef CLEANUP_which
#undef FOR_which
#undef FLAG_a
#endif

// who   a
#undef OPTSTR_who
#define OPTSTR_who "a"
#ifdef CLEANUP_who
#undef CLEANUP_who
#undef FOR_who
#undef FLAG_a
#endif

// xargs ^E:P#optrn#<1(max-args)s#0[!0E] ^E:P#optrn#<1(max-args)s#0[!0E]
#undef OPTSTR_xargs
#define OPTSTR_xargs "^E:P#optrn#<1(max-args)s#0[!0E]"
#ifdef CLEANUP_xargs
#undef CLEANUP_xargs
#undef FOR_xargs
#undef FLAG_0
#undef FLAG_s
#undef FLAG_n
#undef FLAG_r
#undef FLAG_t
#undef FLAG_p
#undef FLAG_o
#undef FLAG_P
#undef FLAG_E
#endif

// xxd >1c#l#o#g#<1=2iprs#[!rs] >1c#l#o#g#<1=2iprs#[!rs]
#undef OPTSTR_xxd
#define OPTSTR_xxd ">1c#l#o#g#<1=2iprs#[!rs]"
#ifdef CLEANUP_xxd
#undef CLEANUP_xxd
#undef FOR_xxd
#undef FLAG_s
#undef FLAG_r
#undef FLAG_p
#undef FLAG_i
#undef FLAG_g
#undef FLAG_o
#undef FLAG_l
#undef FLAG_c
#endif

// xzcat    
#undef OPTSTR_xzcat
#define OPTSTR_xzcat 0
#ifdef CLEANUP_xzcat
#undef CLEANUP_xzcat
#undef FOR_xzcat
#endif

// yes    
#undef OPTSTR_yes
#define OPTSTR_yes 0
#ifdef CLEANUP_yes
#undef CLEANUP_yes
#undef FOR_yes
#endif

// zcat cdfk123456789[-123456789] cdfk123456789[-123456789]
#undef OPTSTR_zcat
#define OPTSTR_zcat "cdfk123456789[-123456789]"
#ifdef CLEANUP_zcat
#undef CLEANUP_zcat
#undef FOR_zcat
#undef FLAG_9
#undef FLAG_8
#undef FLAG_7
#undef FLAG_6
#undef FLAG_5
#undef FLAG_4
#undef FLAG_3
#undef FLAG_2
#undef FLAG_1
#undef FLAG_k
#undef FLAG_f
#undef FLAG_d
#undef FLAG_c
#endif

#ifdef FOR_acpi
#ifndef TT
#define TT this.acpi
#endif
#define FLAG_V (FORCED_FLAG<<0)
#define FLAG_t (FORCED_FLAG<<1)
#define FLAG_c (FORCED_FLAG<<2)
#define FLAG_b (FORCED_FLAG<<3)
#define FLAG_a (FORCED_FLAG<<4)
#endif

#ifdef FOR_arch
#ifndef TT
#define TT this.arch
#endif
#endif

#ifdef FOR_arp
#ifndef TT
#define TT this.arp
#endif
#define FLAG_H (FORCED_FLAG<<0)
#define FLAG_A (FORCED_FLAG<<1)
#define FLAG_p (FORCED_FLAG<<2)
#define FLAG_a (FORCED_FLAG<<3)
#define FLAG_d (FORCED_FLAG<<4)
#define FLAG_s (FORCED_FLAG<<5)
#define FLAG_D (FORCED_FLAG<<6)
#define FLAG_n (FORCED_FLAG<<7)
#define FLAG_i (FORCED_FLAG<<8)
#define FLAG_v (FORCED_FLAG<<9)
#endif

#ifdef FOR_arping
#ifndef TT
#define TT this.arping
#endif
#define FLAG_f (FORCED_FLAG<<0)
#define FLAG_q (FORCED_FLAG<<1)
#define FLAG_b (FORCED_FLAG<<2)
#define FLAG_D (FORCED_FLAG<<3)
#define FLAG_U (FORCED_FLAG<<4)
#define FLAG_A (FORCED_FLAG<<5)
#define FLAG_c (FORCED_FLAG<<6)
#define FLAG_w (FORCED_FLAG<<7)
#define FLAG_I (FORCED_FLAG<<8)
#define FLAG_s (FORCED_FLAG<<9)
#endif

#ifdef FOR_ascii
#ifndef TT
#define TT this.ascii
#endif
#endif

#ifdef FOR_base64
#ifndef TT
#define TT this.base64
#endif
#define FLAG_w (FORCED_FLAG<<0)
#define FLAG_i (FORCED_FLAG<<1)
#define FLAG_d (FORCED_FLAG<<2)
#endif

#ifdef FOR_basename
#ifndef TT
#define TT this.basename
#endif
#define FLAG_s (1<<0)
#define FLAG_a (1<<1)
#endif

#ifdef FOR_bc
#ifndef TT
#define TT this.bc
#endif
#define FLAG_w (FORCED_FLAG<<0)
#define FLAG_s (FORCED_FLAG<<1)
#define FLAG_q (FORCED_FLAG<<2)
#define FLAG_l (FORCED_FLAG<<3)
#define FLAG_i (FORCED_FLAG<<4)
#endif

#ifdef FOR_blkid
#ifndef TT
#define TT this.blkid
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_L (FORCED_FLAG<<1)
#define FLAG_U (FORCED_FLAG<<2)
#endif

#ifdef FOR_blockdev
#ifndef TT
#define TT this.blockdev
#endif
#define FLAG_rereadpt (FORCED_FLAG<<0)
#define FLAG_flushbufs (FORCED_FLAG<<1)
#define FLAG_setra (FORCED_FLAG<<2)
#define FLAG_getra (FORCED_FLAG<<3)
#define FLAG_getsize64 (FORCED_FLAG<<4)
#define FLAG_getsize (FORCED_FLAG<<5)
#define FLAG_getsz (FORCED_FLAG<<6)
#define FLAG_setbsz (FORCED_FLAG<<7)
#define FLAG_getbsz (FORCED_FLAG<<8)
#define FLAG_getss (FORCED_FLAG<<9)
#define FLAG_getro (FORCED_FLAG<<10)
#define FLAG_setrw (FORCED_FLAG<<11)
#define FLAG_setro (FORCED_FLAG<<12)
#endif

#ifdef FOR_bootchartd
#ifndef TT
#define TT this.bootchartd
#endif
#endif

#ifdef FOR_brctl
#ifndef TT
#define TT this.brctl
#endif
#endif

#ifdef FOR_bunzip2
#ifndef TT
#define TT this.bunzip2
#endif
#define FLAG_v (FORCED_FLAG<<0)
#define FLAG_k (FORCED_FLAG<<1)
#define FLAG_t (FORCED_FLAG<<2)
#define FLAG_f (FORCED_FLAG<<3)
#define FLAG_c (FORCED_FLAG<<4)
#endif

#ifdef FOR_bzcat
#ifndef TT
#define TT this.bzcat
#endif
#endif

#ifdef FOR_cal
#ifndef TT
#define TT this.cal
#endif
#define FLAG_h (FORCED_FLAG<<0)
#endif

#ifdef FOR_cat
#ifndef TT
#define TT this.cat
#endif
#define FLAG_e (1<<0)
#define FLAG_t (1<<1)
#define FLAG_v (1<<2)
#define FLAG_u (1<<3)
#endif

#ifdef FOR_catv
#ifndef TT
#define TT this.catv
#endif
#define FLAG_e (FORCED_FLAG<<0)
#define FLAG_t (FORCED_FLAG<<1)
#define FLAG_v (FORCED_FLAG<<2)
#endif

#ifdef FOR_cd
#ifndef TT
#define TT this.cd
#endif
#define FLAG_P (FORCED_FLAG<<0)
#define FLAG_L (FORCED_FLAG<<1)
#endif

#ifdef FOR_chattr
#ifndef TT
#define TT this.chattr
#endif
#define FLAG_R (FORCED_FLAG<<0)
#define FLAG_v (FORCED_FLAG<<1)
#define FLAG_p (FORCED_FLAG<<2)
#endif

#ifdef FOR_chcon
#ifndef TT
#define TT this.chcon
#endif
#define FLAG_R (FORCED_FLAG<<0)
#define FLAG_v (FORCED_FLAG<<1)
#define FLAG_h (FORCED_FLAG<<2)
#endif

#ifdef FOR_chgrp
#ifndef TT
#define TT this.chgrp
#endif
#define FLAG_v (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#define FLAG_R (FORCED_FLAG<<2)
#define FLAG_H (FORCED_FLAG<<3)
#define FLAG_L (FORCED_FLAG<<4)
#define FLAG_P (FORCED_FLAG<<5)
#define FLAG_h (FORCED_FLAG<<6)
#endif

#ifdef FOR_chmod
#ifndef TT
#define TT this.chmod
#endif
#define FLAG_f (1<<0)
#define FLAG_R (1<<1)
#define FLAG_v (1<<2)
#endif

#ifdef FOR_chroot
#ifndef TT
#define TT this.chroot
#endif
#endif

#ifdef FOR_chrt
#ifndef TT
#define TT this.chrt
#endif
#define FLAG_o (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#define FLAG_r (FORCED_FLAG<<2)
#define FLAG_b (FORCED_FLAG<<3)
#define FLAG_R (FORCED_FLAG<<4)
#define FLAG_i (FORCED_FLAG<<5)
#define FLAG_p (FORCED_FLAG<<6)
#define FLAG_m (FORCED_FLAG<<7)
#endif

#ifdef FOR_chvt
#ifndef TT
#define TT this.chvt
#endif
#endif

#ifdef FOR_cksum
#ifndef TT
#define TT this.cksum
#endif
#define FLAG_N (FORCED_FLAG<<0)
#define FLAG_L (FORCED_FLAG<<1)
#define FLAG_P (FORCED_FLAG<<2)
#define FLAG_I (FORCED_FLAG<<3)
#define FLAG_H (FORCED_FLAG<<4)
#endif

#ifdef FOR_clear
#ifndef TT
#define TT this.clear
#endif
#endif

#ifdef FOR_cmp
#ifndef TT
#define TT this.cmp
#endif
#define FLAG_s (1<<0)
#define FLAG_l (1<<1)
#endif

#ifdef FOR_comm
#ifndef TT
#define TT this.comm
#endif
#define FLAG_1 (1<<0)
#define FLAG_2 (1<<1)
#define FLAG_3 (1<<2)
#endif

#ifdef FOR_count
#ifndef TT
#define TT this.count
#endif
#endif

#ifdef FOR_cp
#ifndef TT
#define TT this.cp
#endif
#define FLAG_T (1<<0)
#define FLAG_i (1<<1)
#define FLAG_f (1<<2)
#define FLAG_F (1<<3)
#define FLAG_n (1<<4)
#define FLAG_v (1<<5)
#define FLAG_l (1<<6)
#define FLAG_s (1<<7)
#define FLAG_a (1<<8)
#define FLAG_d (1<<9)
#define FLAG_r (1<<10)
#define FLAG_p (1<<11)
#define FLAG_P (1<<12)
#define FLAG_L (1<<13)
#define FLAG_H (1<<14)
#define FLAG_R (1<<15)
#define FLAG_D (1<<16)
#define FLAG_preserve (1<<17)
#endif

#ifdef FOR_cpio
#ifndef TT
#define TT this.cpio
#endif
#define FLAG_o (FORCED_FLAG<<0)
#define FLAG_v (FORCED_FLAG<<1)
#define FLAG_F (FORCED_FLAG<<2)
#define FLAG_t (FORCED_FLAG<<3)
#define FLAG_i (FORCED_FLAG<<4)
#define FLAG_p (FORCED_FLAG<<5)
#define FLAG_H (FORCED_FLAG<<6)
#define FLAG_u (FORCED_FLAG<<7)
#define FLAG_d (FORCED_FLAG<<8)
#define FLAG_m (FORCED_FLAG<<9)
#define FLAG_trailer (FORCED_FLAG<<10)
#define FLAG_no_preserve_owner (FORCED_FLAG<<11)
#endif

#ifdef FOR_crc32
#ifndef TT
#define TT this.crc32
#endif
#endif

#ifdef FOR_crond
#ifndef TT
#define TT this.crond
#endif
#define FLAG_c (FORCED_FLAG<<0)
#define FLAG_L (FORCED_FLAG<<1)
#define FLAG_d (FORCED_FLAG<<2)
#define FLAG_l (FORCED_FLAG<<3)
#define FLAG_S (FORCED_FLAG<<4)
#define FLAG_b (FORCED_FLAG<<5)
#define FLAG_f (FORCED_FLAG<<6)
#endif

#ifdef FOR_crontab
#ifndef TT
#define TT this.crontab
#endif
#define FLAG_r (FORCED_FLAG<<0)
#define FLAG_l (FORCED_FLAG<<1)
#define FLAG_e (FORCED_FLAG<<2)
#define FLAG_u (FORCED_FLAG<<3)
#define FLAG_c (FORCED_FLAG<<4)
#endif

#ifdef FOR_cut
#ifndef TT
#define TT this.cut
#endif
#define FLAG_n (1<<0)
#define FLAG_D (1<<1)
#define FLAG_s (1<<2)
#define FLAG_d (1<<3)
#define FLAG_O (1<<4)
#define FLAG_C (1<<5)
#define FLAG_F (1<<6)
#define FLAG_f (1<<7)
#define FLAG_c (1<<8)
#define FLAG_b (1<<9)
#endif

#ifdef FOR_date
#ifndef TT
#define TT this.date
#endif
#define FLAG_u (1<<0)
#define FLAG_r (1<<1)
#define FLAG_D (1<<2)
#define FLAG_d (1<<3)
#endif

#ifdef FOR_dd
#ifndef TT
#define TT this.dd
#endif
#endif

#ifdef FOR_deallocvt
#ifndef TT
#define TT this.deallocvt
#endif
#endif

#ifdef FOR_demo_many_options
#ifndef TT
#define TT this.demo_many_options
#endif
#define FLAG_a (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_c (FORCED_FLAG<<2)
#define FLAG_d (FORCED_FLAG<<3)
#define FLAG_e (FORCED_FLAG<<4)
#define FLAG_f (FORCED_FLAG<<5)
#define FLAG_g (FORCED_FLAG<<6)
#define FLAG_h (FORCED_FLAG<<7)
#define FLAG_i (FORCED_FLAG<<8)
#define FLAG_j (FORCED_FLAG<<9)
#define FLAG_k (FORCED_FLAG<<10)
#define FLAG_l (FORCED_FLAG<<11)
#define FLAG_m (FORCED_FLAG<<12)
#define FLAG_n (FORCED_FLAG<<13)
#define FLAG_o (FORCED_FLAG<<14)
#define FLAG_p (FORCED_FLAG<<15)
#define FLAG_q (FORCED_FLAG<<16)
#define FLAG_r (FORCED_FLAG<<17)
#define FLAG_s (FORCED_FLAG<<18)
#define FLAG_t (FORCED_FLAG<<19)
#define FLAG_u (FORCED_FLAG<<20)
#define FLAG_v (FORCED_FLAG<<21)
#define FLAG_w (FORCED_FLAG<<22)
#define FLAG_x (FORCED_FLAG<<23)
#define FLAG_y (FORCED_FLAG<<24)
#define FLAG_z (FORCED_FLAG<<25)
#define FLAG_A (FORCED_FLAG<<26)
#define FLAG_B (FORCED_FLAG<<27)
#define FLAG_C (FORCED_FLAG<<28)
#define FLAG_D (FORCED_FLAG<<29)
#define FLAG_E (FORCED_FLAG<<30)
#define FLAG_F (FORCED_FLAGLL<<31)
#define FLAG_G (FORCED_FLAGLL<<32)
#define FLAG_H (FORCED_FLAGLL<<33)
#define FLAG_I (FORCED_FLAGLL<<34)
#define FLAG_J (FORCED_FLAGLL<<35)
#define FLAG_K (FORCED_FLAGLL<<36)
#define FLAG_L (FORCED_FLAGLL<<37)
#define FLAG_M (FORCED_FLAGLL<<38)
#define FLAG_N (FORCED_FLAGLL<<39)
#define FLAG_O (FORCED_FLAGLL<<40)
#define FLAG_P (FORCED_FLAGLL<<41)
#define FLAG_Q (FORCED_FLAGLL<<42)
#define FLAG_R (FORCED_FLAGLL<<43)
#define FLAG_S (FORCED_FLAGLL<<44)
#define FLAG_T (FORCED_FLAGLL<<45)
#define FLAG_U (FORCED_FLAGLL<<46)
#define FLAG_V (FORCED_FLAGLL<<47)
#define FLAG_W (FORCED_FLAGLL<<48)
#define FLAG_X (FORCED_FLAGLL<<49)
#define FLAG_Y (FORCED_FLAGLL<<50)
#define FLAG_Z (FORCED_FLAGLL<<51)
#endif

#ifdef FOR_demo_number
#ifndef TT
#define TT this.demo_number
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_d (FORCED_FLAG<<2)
#define FLAG_h (FORCED_FLAG<<3)
#define FLAG_D (FORCED_FLAG<<4)
#endif

#ifdef FOR_demo_scankey
#ifndef TT
#define TT this.demo_scankey
#endif
#endif

#ifdef FOR_demo_utf8towc
#ifndef TT
#define TT this.demo_utf8towc
#endif
#endif

#ifdef FOR_devmem
#ifndef TT
#define TT this.devmem
#endif
#endif

#ifdef FOR_df
#ifndef TT
#define TT this.df
#endif
#define FLAG_a (FORCED_FLAG<<0)
#define FLAG_t (FORCED_FLAG<<1)
#define FLAG_i (FORCED_FLAG<<2)
#define FLAG_h (FORCED_FLAG<<3)
#define FLAG_k (FORCED_FLAG<<4)
#define FLAG_P (FORCED_FLAG<<5)
#define FLAG_H (FORCED_FLAG<<6)
#endif

#ifdef FOR_dhcp
#ifndef TT
#define TT this.dhcp
#endif
#define FLAG_f (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_n (FORCED_FLAG<<2)
#define FLAG_q (FORCED_FLAG<<3)
#define FLAG_v (FORCED_FLAG<<4)
#define FLAG_o (FORCED_FLAG<<5)
#define FLAG_a (FORCED_FLAG<<6)
#define FLAG_C (FORCED_FLAG<<7)
#define FLAG_R (FORCED_FLAG<<8)
#define FLAG_B (FORCED_FLAG<<9)
#define FLAG_S (FORCED_FLAG<<10)
#define FLAG_i (FORCED_FLAG<<11)
#define FLAG_p (FORCED_FLAG<<12)
#define FLAG_s (FORCED_FLAG<<13)
#define FLAG_t (FORCED_FLAG<<14)
#define FLAG_T (FORCED_FLAG<<15)
#define FLAG_A (FORCED_FLAG<<16)
#define FLAG_O (FORCED_FLAG<<17)
#define FLAG_r (FORCED_FLAG<<18)
#define FLAG_x (FORCED_FLAG<<19)
#define FLAG_F (FORCED_FLAG<<20)
#define FLAG_H (FORCED_FLAG<<21)
#define FLAG_V (FORCED_FLAG<<22)
#endif

#ifdef FOR_dhcp6
#ifndef TT
#define TT this.dhcp6
#endif
#define FLAG_f (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_n (FORCED_FLAG<<2)
#define FLAG_q (FORCED_FLAG<<3)
#define FLAG_v (FORCED_FLAG<<4)
#define FLAG_R (FORCED_FLAG<<5)
#define FLAG_S (FORCED_FLAG<<6)
#define FLAG_i (FORCED_FLAG<<7)
#define FLAG_p (FORCED_FLAG<<8)
#define FLAG_s (FORCED_FLAG<<9)
#define FLAG_t (FORCED_FLAG<<10)
#define FLAG_T (FORCED_FLAG<<11)
#define FLAG_A (FORCED_FLAG<<12)
#define FLAG_r (FORCED_FLAG<<13)
#endif

#ifdef FOR_dhcpd
#ifndef TT
#define TT this.dhcpd
#endif
#define FLAG_6 (FORCED_FLAG<<0)
#define FLAG_4 (FORCED_FLAG<<1)
#define FLAG_S (FORCED_FLAG<<2)
#define FLAG_i (FORCED_FLAG<<3)
#define FLAG_f (FORCED_FLAG<<4)
#define FLAG_P (FORCED_FLAG<<5)
#endif

#ifdef FOR_diff
#ifndef TT
#define TT this.diff
#endif
#define FLAG_U (1<<0)
#define FLAG_r (1<<1)
#define FLAG_N (1<<2)
#define FLAG_S (1<<3)
#define FLAG_L (1<<4)
#define FLAG_a (1<<5)
#define FLAG_q (1<<6)
#define FLAG_s (1<<7)
#define FLAG_T (1<<8)
#define FLAG_i (1<<9)
#define FLAG_w (1<<10)
#define FLAG_t (1<<11)
#define FLAG_u (1<<12)
#define FLAG_b (1<<13)
#define FLAG_d (1<<14)
#define FLAG_B (1<<15)
#define FLAG_strip_trailing_cr (1<<16)
#define FLAG_color (1<<17)
#endif

#ifdef FOR_dirname
#ifndef TT
#define TT this.dirname
#endif
#endif

#ifdef FOR_dmesg
#ifndef TT
#define TT this.dmesg
#endif
#define FLAG_c (FORCED_FLAG<<0)
#define FLAG_n (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#define FLAG_r (FORCED_FLAG<<3)
#define FLAG_t (FORCED_FLAG<<4)
#define FLAG_T (FORCED_FLAG<<5)
#define FLAG_S (FORCED_FLAG<<6)
#define FLAG_C (FORCED_FLAG<<7)
#define FLAG_w (FORCED_FLAG<<8)
#endif

#ifdef FOR_dnsdomainname
#ifndef TT
#define TT this.dnsdomainname
#endif
#endif

#ifdef FOR_dos2unix
#ifndef TT
#define TT this.dos2unix
#endif
#endif

#ifdef FOR_du
#ifndef TT
#define TT this.du
#endif
#define FLAG_x (1<<0)
#define FLAG_s (1<<1)
#define FLAG_L (1<<2)
#define FLAG_K (1<<3)
#define FLAG_k (1<<4)
#define FLAG_H (1<<5)
#define FLAG_a (1<<6)
#define FLAG_c (1<<7)
#define FLAG_l (1<<8)
#define FLAG_m (1<<9)
#define FLAG_h (1<<10)
#define FLAG_d (1<<11)
#endif

#ifdef FOR_dumpleases
#ifndef TT
#define TT this.dumpleases
#endif
#define FLAG_f (FORCED_FLAG<<0)
#define FLAG_r (FORCED_FLAG<<1)
#define FLAG_a (FORCED_FLAG<<2)
#endif

#ifdef FOR_echo
#ifndef TT
#define TT this.echo
#endif
#define FLAG_n (1<<0)
#define FLAG_e (1<<1)
#define FLAG_E (1<<2)
#endif

#ifdef FOR_eject
#ifndef TT
#define TT this.eject
#endif
#define FLAG_T (FORCED_FLAG<<0)
#define FLAG_t (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#endif

#ifdef FOR_env
#ifndef TT
#define TT this.env
#endif
#define FLAG_u (1<<0)
#define FLAG_i (1<<1)
#define FLAG_0 (1<<2)
#endif

#ifdef FOR_exit
#ifndef TT
#define TT this.exit
#endif
#endif

#ifdef FOR_expand
#ifndef TT
#define TT this.expand
#endif
#define FLAG_t (FORCED_FLAG<<0)
#endif

#ifdef FOR_expr
#ifndef TT
#define TT this.expr
#endif
#endif

#ifdef FOR_factor
#ifndef TT
#define TT this.factor
#endif
#endif

#ifdef FOR_fallocate
#ifndef TT
#define TT this.fallocate
#endif
#define FLAG_o (FORCED_FLAG<<0)
#define FLAG_l (FORCED_FLAG<<1)
#endif

#ifdef FOR_false
#ifndef TT
#define TT this.false
#endif
#endif

#ifdef FOR_fdisk
#ifndef TT
#define TT this.fdisk
#endif
#define FLAG_l (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_b (FORCED_FLAG<<2)
#define FLAG_S (FORCED_FLAG<<3)
#define FLAG_H (FORCED_FLAG<<4)
#define FLAG_C (FORCED_FLAG<<5)
#endif

#ifdef FOR_file
#ifndef TT
#define TT this.file
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_L (FORCED_FLAG<<1)
#define FLAG_h (FORCED_FLAG<<2)
#define FLAG_b (FORCED_FLAG<<3)
#endif

#ifdef FOR_find
#ifndef TT
#define TT this.find
#endif
#define FLAG_L (1<<0)
#define FLAG_H (1<<1)
#endif

#ifdef FOR_flock
#ifndef TT
#define TT this.flock
#endif
#define FLAG_x (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#define FLAG_n (FORCED_FLAG<<3)
#endif

#ifdef FOR_fmt
#ifndef TT
#define TT this.fmt
#endif
#define FLAG_w (FORCED_FLAG<<0)
#endif

#ifdef FOR_fold
#ifndef TT
#define TT this.fold
#endif
#define FLAG_w (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#define FLAG_b (FORCED_FLAG<<3)
#endif

#ifdef FOR_free
#ifndef TT
#define TT this.free
#endif
#define FLAG_b (FORCED_FLAG<<0)
#define FLAG_k (FORCED_FLAG<<1)
#define FLAG_m (FORCED_FLAG<<2)
#define FLAG_g (FORCED_FLAG<<3)
#define FLAG_t (FORCED_FLAG<<4)
#define FLAG_h (FORCED_FLAG<<5)
#endif

#ifdef FOR_freeramdisk
#ifndef TT
#define TT this.freeramdisk
#endif
#endif

#ifdef FOR_fsck
#ifndef TT
#define TT this.fsck
#endif
#define FLAG_C (FORCED_FLAG<<0)
#define FLAG_s (FORCED_FLAG<<1)
#define FLAG_V (FORCED_FLAG<<2)
#define FLAG_T (FORCED_FLAG<<3)
#define FLAG_R (FORCED_FLAG<<4)
#define FLAG_P (FORCED_FLAG<<5)
#define FLAG_N (FORCED_FLAG<<6)
#define FLAG_A (FORCED_FLAG<<7)
#define FLAG_t (FORCED_FLAG<<8)
#endif

#ifdef FOR_fsfreeze
#ifndef TT
#define TT this.fsfreeze
#endif
#define FLAG_u (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#endif

#ifdef FOR_fstype
#ifndef TT
#define TT this.fstype
#endif
#endif

#ifdef FOR_fsync
#ifndef TT
#define TT this.fsync
#endif
#define FLAG_d (FORCED_FLAG<<0)
#endif

#ifdef FOR_ftpget
#ifndef TT
#define TT this.ftpget
#endif
#define FLAG_D (FORCED_FLAG<<0)
#define FLAG_d (FORCED_FLAG<<1)
#define FLAG_M (FORCED_FLAG<<2)
#define FLAG_m (FORCED_FLAG<<3)
#define FLAG_L (FORCED_FLAG<<4)
#define FLAG_l (FORCED_FLAG<<5)
#define FLAG_s (FORCED_FLAG<<6)
#define FLAG_g (FORCED_FLAG<<7)
#define FLAG_v (FORCED_FLAG<<8)
#define FLAG_u (FORCED_FLAG<<9)
#define FLAG_p (FORCED_FLAG<<10)
#define FLAG_c (FORCED_FLAG<<11)
#define FLAG_P (FORCED_FLAG<<12)
#endif

#ifdef FOR_getconf
#ifndef TT
#define TT this.getconf
#endif
#define FLAG_l (1<<0)
#define FLAG_a (1<<1)
#endif

#ifdef FOR_getenforce
#ifndef TT
#define TT this.getenforce
#endif
#endif

#ifdef FOR_getfattr
#ifndef TT
#define TT this.getfattr
#endif
#define FLAG_n (FORCED_FLAG<<0)
#define FLAG_h (FORCED_FLAG<<1)
#define FLAG_d (FORCED_FLAG<<2)
#define FLAG_only_values (FORCED_FLAG<<3)
#endif

#ifdef FOR_getopt
#ifndef TT
#define TT this.getopt
#endif
#define FLAG_u (1<<0)
#define FLAG_T (1<<1)
#define FLAG_l (1<<2)
#define FLAG_o (1<<3)
#define FLAG_n (1<<4)
#define FLAG_a (1<<5)
#endif

#ifdef FOR_getty
#ifndef TT
#define TT this.getty
#endif
#define FLAG_h (FORCED_FLAG<<0)
#define FLAG_L (FORCED_FLAG<<1)
#define FLAG_m (FORCED_FLAG<<2)
#define FLAG_n (FORCED_FLAG<<3)
#define FLAG_w (FORCED_FLAG<<4)
#define FLAG_i (FORCED_FLAG<<5)
#define FLAG_f (FORCED_FLAG<<6)
#define FLAG_l (FORCED_FLAG<<7)
#define FLAG_I (FORCED_FLAG<<8)
#define FLAG_H (FORCED_FLAG<<9)
#define FLAG_t (FORCED_FLAG<<10)
#endif

#ifdef FOR_grep
#ifndef TT
#define TT this.grep
#endif
#define FLAG_x (1<<0)
#define FLAG_m (1<<1)
#define FLAG_A (1<<2)
#define FLAG_B (1<<3)
#define FLAG_C (1<<4)
#define FLAG_f (1<<5)
#define FLAG_e (1<<6)
#define FLAG_q (1<<7)
#define FLAG_l (1<<8)
#define FLAG_c (1<<9)
#define FLAG_w (1<<10)
#define FLAG_v (1<<11)
#define FLAG_s (1<<12)
#define FLAG_R (1<<13)
#define FLAG_r (1<<14)
#define FLAG_o (1<<15)
#define FLAG_n (1<<16)
#define FLAG_i (1<<17)
#define FLAG_h (1<<18)
#define FLAG_b (1<<19)
#define FLAG_a (1<<20)
#define FLAG_I (1<<21)
#define FLAG_H (1<<22)
#define FLAG_F (1<<23)
#define FLAG_E (1<<24)
#define FLAG_z (1<<25)
#define FLAG_Z (1<<26)
#define FLAG_M (1<<27)
#define FLAG_S (1<<28)
#define FLAG_exclude_dir (1<<29)
#define FLAG_color (1<<30)
#define FLAG_line_buffered (1LL<<31)
#endif

#ifdef FOR_groupadd
#ifndef TT
#define TT this.groupadd
#endif
#define FLAG_S (FORCED_FLAG<<0)
#define FLAG_g (FORCED_FLAG<<1)
#endif

#ifdef FOR_groupdel
#ifndef TT
#define TT this.groupdel
#endif
#endif

#ifdef FOR_groups
#ifndef TT
#define TT this.groups
#endif
#endif

#ifdef FOR_gunzip
#ifndef TT
#define TT this.gunzip
#endif
#define FLAG_9 (FORCED_FLAG<<0)
#define FLAG_8 (FORCED_FLAG<<1)
#define FLAG_7 (FORCED_FLAG<<2)
#define FLAG_6 (FORCED_FLAG<<3)
#define FLAG_5 (FORCED_FLAG<<4)
#define FLAG_4 (FORCED_FLAG<<5)
#define FLAG_3 (FORCED_FLAG<<6)
#define FLAG_2 (FORCED_FLAG<<7)
#define FLAG_1 (FORCED_FLAG<<8)
#define FLAG_k (FORCED_FLAG<<9)
#define FLAG_f (FORCED_FLAG<<10)
#define FLAG_d (FORCED_FLAG<<11)
#define FLAG_c (FORCED_FLAG<<12)
#endif

#ifdef FOR_gzip
#ifndef TT
#define TT this.gzip
#endif
#define FLAG_9 (1<<0)
#define FLAG_8 (1<<1)
#define FLAG_7 (1<<2)
#define FLAG_6 (1<<3)
#define FLAG_5 (1<<4)
#define FLAG_4 (1<<5)
#define FLAG_3 (1<<6)
#define FLAG_2 (1<<7)
#define FLAG_1 (1<<8)
#define FLAG_k (1<<9)
#define FLAG_f (1<<10)
#define FLAG_d (1<<11)
#define FLAG_c (1<<12)
#define FLAG_n (1<<13)
#endif

#ifdef FOR_head
#ifndef TT
#define TT this.head
#endif
#define FLAG_v (1<<0)
#define FLAG_q (1<<1)
#define FLAG_c (1<<2)
#define FLAG_n (1<<3)
#endif

#ifdef FOR_hello
#ifndef TT
#define TT this.hello
#endif
#endif

#ifdef FOR_help
#ifndef TT
#define TT this.help
#endif
#define FLAG_u (FORCED_FLAG<<0)
#define FLAG_h (FORCED_FLAG<<1)
#define FLAG_a (FORCED_FLAG<<2)
#endif

#ifdef FOR_hexedit
#ifndef TT
#define TT this.hexedit
#endif
#define FLAG_r (FORCED_FLAG<<0)
#endif

#ifdef FOR_host
#ifndef TT
#define TT this.host
#endif
#define FLAG_t (FORCED_FLAG<<0)
#define FLAG_v (FORCED_FLAG<<1)
#define FLAG_a (FORCED_FLAG<<2)
#endif

#ifdef FOR_hostid
#ifndef TT
#define TT this.hostid
#endif
#endif

#ifdef FOR_hostname
#ifndef TT
#define TT this.hostname
#endif
#define FLAG_F (1<<0)
#define FLAG_f (1<<1)
#define FLAG_s (1<<2)
#define FLAG_d (1<<3)
#define FLAG_b (1<<4)
#endif

#ifdef FOR_hwclock
#ifndef TT
#define TT this.hwclock
#endif
#define FLAG_w (FORCED_FLAG<<0)
#define FLAG_r (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#define FLAG_t (FORCED_FLAG<<3)
#define FLAG_l (FORCED_FLAG<<4)
#define FLAG_u (FORCED_FLAG<<5)
#define FLAG_f (FORCED_FLAG<<6)
#define FLAG_fast (FORCED_FLAG<<7)
#endif

#ifdef FOR_i2cdetect
#ifndef TT
#define TT this.i2cdetect
#endif
#define FLAG_y (FORCED_FLAG<<0)
#define FLAG_l (FORCED_FLAG<<1)
#define FLAG_F (FORCED_FLAG<<2)
#define FLAG_a (FORCED_FLAG<<3)
#endif

#ifdef FOR_i2cdump
#ifndef TT
#define TT this.i2cdump
#endif
#define FLAG_y (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#endif

#ifdef FOR_i2cget
#ifndef TT
#define TT this.i2cget
#endif
#define FLAG_y (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#endif

#ifdef FOR_i2cset
#ifndef TT
#define TT this.i2cset
#endif
#define FLAG_y (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#endif

#ifdef FOR_iconv
#ifndef TT
#define TT this.iconv
#endif
#define FLAG_f (FORCED_FLAG<<0)
#define FLAG_t (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#define FLAG_c (FORCED_FLAG<<3)
#endif

#ifdef FOR_id
#ifndef TT
#define TT this.id
#endif
#define FLAG_u (1<<0)
#define FLAG_r (1<<1)
#define FLAG_g (1<<2)
#define FLAG_G (1<<3)
#define FLAG_n (1<<4)
#define FLAG_Z (FORCED_FLAG<<5)
#endif

#ifdef FOR_ifconfig
#ifndef TT
#define TT this.ifconfig
#endif
#define FLAG_S (FORCED_FLAG<<0)
#define FLAG_a (FORCED_FLAG<<1)
#endif

#ifdef FOR_init
#ifndef TT
#define TT this.init
#endif
#endif

#ifdef FOR_inotifyd
#ifndef TT
#define TT this.inotifyd
#endif
#endif

#ifdef FOR_insmod
#ifndef TT
#define TT this.insmod
#endif
#endif

#ifdef FOR_install
#ifndef TT
#define TT this.install
#endif
#define FLAG_g (FORCED_FLAG<<0)
#define FLAG_o (FORCED_FLAG<<1)
#define FLAG_m (FORCED_FLAG<<2)
#define FLAG_v (FORCED_FLAG<<3)
#define FLAG_s (FORCED_FLAG<<4)
#define FLAG_p (FORCED_FLAG<<5)
#define FLAG_D (FORCED_FLAG<<6)
#define FLAG_d (FORCED_FLAG<<7)
#define FLAG_c (FORCED_FLAG<<8)
#endif

#ifdef FOR_ionice
#ifndef TT
#define TT this.ionice
#endif
#define FLAG_p (FORCED_FLAG<<0)
#define FLAG_n (FORCED_FLAG<<1)
#define FLAG_c (FORCED_FLAG<<2)
#define FLAG_t (FORCED_FLAG<<3)
#endif

#ifdef FOR_iorenice
#ifndef TT
#define TT this.iorenice
#endif
#endif

#ifdef FOR_iotop
#ifndef TT
#define TT this.iotop
#endif
#define FLAG_q (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_n (FORCED_FLAG<<2)
#define FLAG_m (FORCED_FLAG<<3)
#define FLAG_d (FORCED_FLAG<<4)
#define FLAG_s (FORCED_FLAG<<5)
#define FLAG_u (FORCED_FLAG<<6)
#define FLAG_p (FORCED_FLAG<<7)
#define FLAG_o (FORCED_FLAG<<8)
#define FLAG_k (FORCED_FLAG<<9)
#define FLAG_H (FORCED_FLAG<<10)
#define FLAG_O (FORCED_FLAG<<11)
#define FLAG_K (FORCED_FLAG<<12)
#define FLAG_a (FORCED_FLAG<<13)
#define FLAG_A (FORCED_FLAG<<14)
#endif

#ifdef FOR_ip
#ifndef TT
#define TT this.ip
#endif
#endif

#ifdef FOR_ipcrm
#ifndef TT
#define TT this.ipcrm
#endif
#define FLAG_Q (FORCED_FLAG<<0)
#define FLAG_q (FORCED_FLAG<<1)
#define FLAG_S (FORCED_FLAG<<2)
#define FLAG_s (FORCED_FLAG<<3)
#define FLAG_M (FORCED_FLAG<<4)
#define FLAG_m (FORCED_FLAG<<5)
#endif

#ifdef FOR_ipcs
#ifndef TT
#define TT this.ipcs
#endif
#define FLAG_i (FORCED_FLAG<<0)
#define FLAG_m (FORCED_FLAG<<1)
#define FLAG_q (FORCED_FLAG<<2)
#define FLAG_s (FORCED_FLAG<<3)
#define FLAG_l (FORCED_FLAG<<4)
#define FLAG_u (FORCED_FLAG<<5)
#define FLAG_t (FORCED_FLAG<<6)
#define FLAG_p (FORCED_FLAG<<7)
#define FLAG_c (FORCED_FLAG<<8)
#define FLAG_a (FORCED_FLAG<<9)
#endif

#ifdef FOR_kill
#ifndef TT
#define TT this.kill
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_l (FORCED_FLAG<<1)
#endif

#ifdef FOR_killall
#ifndef TT
#define TT this.killall
#endif
#define FLAG_w (FORCED_FLAG<<0)
#define FLAG_v (FORCED_FLAG<<1)
#define FLAG_q (FORCED_FLAG<<2)
#define FLAG_l (FORCED_FLAG<<3)
#define FLAG_i (FORCED_FLAG<<4)
#define FLAG_s (FORCED_FLAG<<5)
#endif

#ifdef FOR_killall5
#ifndef TT
#define TT this.killall5
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_l (FORCED_FLAG<<1)
#define FLAG_o (FORCED_FLAG<<2)
#endif

#ifdef FOR_klogd
#ifndef TT
#define TT this.klogd
#endif
#define FLAG_n (FORCED_FLAG<<0)
#define FLAG_c (FORCED_FLAG<<1)
#endif

#ifdef FOR_last
#ifndef TT
#define TT this.last
#endif
#define FLAG_W (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#endif

#ifdef FOR_link
#ifndef TT
#define TT this.link
#endif
#endif

#ifdef FOR_ln
#ifndef TT
#define TT this.ln
#endif
#define FLAG_s (1<<0)
#define FLAG_f (1<<1)
#define FLAG_n (1<<2)
#define FLAG_v (1<<3)
#define FLAG_T (1<<4)
#define FLAG_t (1<<5)
#define FLAG_r (1<<6)
#endif

#ifdef FOR_load_policy
#ifndef TT
#define TT this.load_policy
#endif
#endif

#ifdef FOR_log
#ifndef TT
#define TT this.log
#endif
#define FLAG_t (FORCED_FLAG<<0)
#define FLAG_p (FORCED_FLAG<<1)
#endif

#ifdef FOR_logger
#ifndef TT
#define TT this.logger
#endif
#define FLAG_p (FORCED_FLAG<<0)
#define FLAG_t (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#endif

#ifdef FOR_login
#ifndef TT
#define TT this.login
#endif
#define FLAG_h (FORCED_FLAG<<0)
#define FLAG_p (FORCED_FLAG<<1)
#define FLAG_f (FORCED_FLAG<<2)
#endif

#ifdef FOR_logname
#ifndef TT
#define TT this.logname
#endif
#endif

#ifdef FOR_logwrapper
#ifndef TT
#define TT this.logwrapper
#endif
#endif

#ifdef FOR_losetup
#ifndef TT
#define TT this.losetup
#endif
#define FLAG_D (FORCED_FLAG<<0)
#define FLAG_a (FORCED_FLAG<<1)
#define FLAG_c (FORCED_FLAG<<2)
#define FLAG_d (FORCED_FLAG<<3)
#define FLAG_f (FORCED_FLAG<<4)
#define FLAG_j (FORCED_FLAG<<5)
#define FLAG_o (FORCED_FLAG<<6)
#define FLAG_r (FORCED_FLAG<<7)
#define FLAG_s (FORCED_FLAG<<8)
#define FLAG_S (FORCED_FLAG<<9)
#endif

#ifdef FOR_ls
#ifndef TT
#define TT this.ls
#endif
#define FLAG_1 (1<<0)
#define FLAG_x (1<<1)
#define FLAG_w (1<<2)
#define FLAG_u (1<<3)
#define FLAG_t (1<<4)
#define FLAG_s (1<<5)
#define FLAG_r (1<<6)
#define FLAG_q (1<<7)
#define FLAG_p (1<<8)
#define FLAG_n (1<<9)
#define FLAG_m (1<<10)
#define FLAG_l (1<<11)
#define FLAG_k (1<<12)
#define FLAG_i (1<<13)
#define FLAG_h (1<<14)
#define FLAG_f (1<<15)
#define FLAG_d (1<<16)
#define FLAG_c (1<<17)
#define FLAG_b (1<<18)
#define FLAG_a (1<<19)
#define FLAG_S (1<<20)
#define FLAG_R (1<<21)
#define FLAG_L (1<<22)
#define FLAG_H (1<<23)
#define FLAG_F (1<<24)
#define FLAG_C (1<<25)
#define FLAG_A (1<<26)
#define FLAG_o (1<<27)
#define FLAG_g (1<<28)
#define FLAG_Z (1<<29)
#define FLAG_show_control_chars (1<<30)
#define FLAG_full_time (1LL<<31)
#define FLAG_color (1LL<<32)
#endif

#ifdef FOR_lsattr
#ifndef TT
#define TT this.lsattr
#endif
#define FLAG_R (FORCED_FLAG<<0)
#define FLAG_v (FORCED_FLAG<<1)
#define FLAG_p (FORCED_FLAG<<2)
#define FLAG_a (FORCED_FLAG<<3)
#define FLAG_d (FORCED_FLAG<<4)
#define FLAG_l (FORCED_FLAG<<5)
#endif

#ifdef FOR_lsmod
#ifndef TT
#define TT this.lsmod
#endif
#endif

#ifdef FOR_lsof
#ifndef TT
#define TT this.lsof
#endif
#define FLAG_t (FORCED_FLAG<<0)
#define FLAG_p (FORCED_FLAG<<1)
#define FLAG_l (FORCED_FLAG<<2)
#endif

#ifdef FOR_lspci
#ifndef TT
#define TT this.lspci
#endif
#define FLAG_i (FORCED_FLAG<<0)
#define FLAG_n (FORCED_FLAG<<1)
#define FLAG_k (FORCED_FLAG<<2)
#define FLAG_m (FORCED_FLAG<<3)
#define FLAG_e (FORCED_FLAG<<4)
#endif

#ifdef FOR_lsusb
#ifndef TT
#define TT this.lsusb
#endif
#endif

#ifdef FOR_makedevs
#ifndef TT
#define TT this.makedevs
#endif
#define FLAG_d (FORCED_FLAG<<0)
#endif

#ifdef FOR_man
#ifndef TT
#define TT this.man
#endif
#define FLAG_M (FORCED_FLAG<<0)
#define FLAG_k (FORCED_FLAG<<1)
#endif

#ifdef FOR_mcookie
#ifndef TT
#define TT this.mcookie
#endif
#define FLAG_V (FORCED_FLAG<<0)
#define FLAG_v (FORCED_FLAG<<1)
#endif

#ifdef FOR_md5sum
#ifndef TT
#define TT this.md5sum
#endif
#define FLAG_s (1<<0)
#define FLAG_c (1<<1)
#define FLAG_b (1<<2)
#endif

#ifdef FOR_mdev
#ifndef TT
#define TT this.mdev
#endif
#define FLAG_s (FORCED_FLAG<<0)
#endif

#ifdef FOR_microcom
#ifndef TT
#define TT this.microcom
#endif
#define FLAG_X (1<<0)
#define FLAG_s (1<<1)
#endif

#ifdef FOR_mix
#ifndef TT
#define TT this.mix
#endif
#define FLAG_r (FORCED_FLAG<<0)
#define FLAG_l (FORCED_FLAG<<1)
#define FLAG_d (FORCED_FLAG<<2)
#define FLAG_c (FORCED_FLAG<<3)
#endif

#ifdef FOR_mkdir
#ifndef TT
#define TT this.mkdir
#endif
#define FLAG_m (1<<0)
#define FLAG_p (1<<1)
#define FLAG_v (1<<2)
#define FLAG_Z (FORCED_FLAG<<3)
#endif

#ifdef FOR_mke2fs
#ifndef TT
#define TT this.mke2fs
#endif
#define FLAG_b (FORCED_FLAG<<0)
#define FLAG_i (FORCED_FLAG<<1)
#define FLAG_N (FORCED_FLAG<<2)
#define FLAG_m (FORCED_FLAG<<3)
#define FLAG_q (FORCED_FLAG<<4)
#define FLAG_n (FORCED_FLAG<<5)
#define FLAG_F (FORCED_FLAG<<6)
#define FLAG_g (FORCED_FLAG<<7)
#endif

#ifdef FOR_mkfifo
#ifndef TT
#define TT this.mkfifo
#endif
#define FLAG_m (FORCED_FLAG<<0)
#define FLAG_Z (FORCED_FLAG<<1)
#endif

#ifdef FOR_mknod
#ifndef TT
#define TT this.mknod
#endif
#define FLAG_Z (FORCED_FLAG<<0)
#define FLAG_m (FORCED_FLAG<<1)
#endif

#ifdef FOR_mkpasswd
#ifndef TT
#define TT this.mkpasswd
#endif
#define FLAG_P (FORCED_FLAG<<0)
#define FLAG_m (FORCED_FLAG<<1)
#define FLAG_S (FORCED_FLAG<<2)
#endif

#ifdef FOR_mkswap
#ifndef TT
#define TT this.mkswap
#endif
#define FLAG_L (FORCED_FLAG<<0)
#endif

#ifdef FOR_mktemp
#ifndef TT
#define TT this.mktemp
#endif
#define FLAG_t (1<<0)
#define FLAG_p (1<<1)
#define FLAG_d (1<<2)
#define FLAG_q (1<<3)
#define FLAG_u (1<<4)
#define FLAG_tmpdir (1<<5)
#endif

#ifdef FOR_modinfo
#ifndef TT
#define TT this.modinfo
#endif
#define FLAG_0 (FORCED_FLAG<<0)
#define FLAG_F (FORCED_FLAG<<1)
#define FLAG_k (FORCED_FLAG<<2)
#define FLAG_b (FORCED_FLAG<<3)
#endif

#ifdef FOR_modprobe
#ifndef TT
#define TT this.modprobe
#endif
#define FLAG_d (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_D (FORCED_FLAG<<2)
#define FLAG_s (FORCED_FLAG<<3)
#define FLAG_v (FORCED_FLAG<<4)
#define FLAG_q (FORCED_FLAG<<5)
#define FLAG_r (FORCED_FLAG<<6)
#define FLAG_l (FORCED_FLAG<<7)
#define FLAG_a (FORCED_FLAG<<8)
#endif

#ifdef FOR_more
#ifndef TT
#define TT this.more
#endif
#endif

#ifdef FOR_mount
#ifndef TT
#define TT this.mount
#endif
#define FLAG_o (FORCED_FLAG<<0)
#define FLAG_t (FORCED_FLAG<<1)
#define FLAG_w (FORCED_FLAG<<2)
#define FLAG_v (FORCED_FLAG<<3)
#define FLAG_r (FORCED_FLAG<<4)
#define FLAG_n (FORCED_FLAG<<5)
#define FLAG_f (FORCED_FLAG<<6)
#define FLAG_a (FORCED_FLAG<<7)
#define FLAG_O (FORCED_FLAG<<8)
#endif

#ifdef FOR_mountpoint
#ifndef TT
#define TT this.mountpoint
#endif
#define FLAG_x (FORCED_FLAG<<0)
#define FLAG_d (FORCED_FLAG<<1)
#define FLAG_q (FORCED_FLAG<<2)
#endif

#ifdef FOR_mv
#ifndef TT
#define TT this.mv
#endif
#define FLAG_T (1<<0)
#define FLAG_i (1<<1)
#define FLAG_f (1<<2)
#define FLAG_F (1<<3)
#define FLAG_n (1<<4)
#define FLAG_v (1<<5)
#endif

#ifdef FOR_nbd_client
#ifndef TT
#define TT this.nbd_client
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_n (FORCED_FLAG<<1)
#endif

#ifdef FOR_netcat
#ifndef TT
#define TT this.netcat
#endif
#define FLAG_U (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_6 (FORCED_FLAG<<2)
#define FLAG_4 (FORCED_FLAG<<3)
#define FLAG_f (FORCED_FLAG<<4)
#define FLAG_s (FORCED_FLAG<<5)
#define FLAG_q (FORCED_FLAG<<6)
#define FLAG_p (FORCED_FLAG<<7)
#define FLAG_W (FORCED_FLAG<<8)
#define FLAG_w (FORCED_FLAG<<9)
#define FLAG_L (FORCED_FLAG<<10)
#define FLAG_l (FORCED_FLAG<<11)
#define FLAG_t (FORCED_FLAG<<12)
#endif

#ifdef FOR_netstat
#ifndef TT
#define TT this.netstat
#endif
#define FLAG_l (FORCED_FLAG<<0)
#define FLAG_a (FORCED_FLAG<<1)
#define FLAG_e (FORCED_FLAG<<2)
#define FLAG_n (FORCED_FLAG<<3)
#define FLAG_t (FORCED_FLAG<<4)
#define FLAG_u (FORCED_FLAG<<5)
#define FLAG_w (FORCED_FLAG<<6)
#define FLAG_x (FORCED_FLAG<<7)
#define FLAG_r (FORCED_FLAG<<8)
#define FLAG_W (FORCED_FLAG<<9)
#define FLAG_p (FORCED_FLAG<<10)
#endif

#ifdef FOR_nice
#ifndef TT
#define TT this.nice
#endif
#define FLAG_n (FORCED_FLAG<<0)
#endif

#ifdef FOR_nl
#ifndef TT
#define TT this.nl
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_n (FORCED_FLAG<<1)
#define FLAG_b (FORCED_FLAG<<2)
#define FLAG_E (FORCED_FLAG<<3)
#define FLAG_w (FORCED_FLAG<<4)
#define FLAG_l (FORCED_FLAG<<5)
#define FLAG_v (FORCED_FLAG<<6)
#endif

#ifdef FOR_nohup
#ifndef TT
#define TT this.nohup
#endif
#endif

#ifdef FOR_nproc
#ifndef TT
#define TT this.nproc
#endif
#define FLAG_all (FORCED_FLAG<<0)
#endif

#ifdef FOR_nsenter
#ifndef TT
#define TT this.nsenter
#endif
#define FLAG_U (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_p (FORCED_FLAG<<2)
#define FLAG_n (FORCED_FLAG<<3)
#define FLAG_m (FORCED_FLAG<<4)
#define FLAG_i (FORCED_FLAG<<5)
#define FLAG_t (FORCED_FLAG<<6)
#define FLAG_F (FORCED_FLAG<<7)
#endif

#ifdef FOR_od
#ifndef TT
#define TT this.od
#endif
#define FLAG_t (1<<0)
#define FLAG_A (1<<1)
#define FLAG_b (1<<2)
#define FLAG_c (1<<3)
#define FLAG_d (1<<4)
#define FLAG_o (1<<5)
#define FLAG_s (1<<6)
#define FLAG_x (1<<7)
#define FLAG_N (1<<8)
#define FLAG_w (1<<9)
#define FLAG_v (1<<10)
#define FLAG_j (1<<11)
#endif

#ifdef FOR_oneit
#ifndef TT
#define TT this.oneit
#endif
#define FLAG_3 (FORCED_FLAG<<0)
#define FLAG_p (FORCED_FLAG<<1)
#define FLAG_c (FORCED_FLAG<<2)
#define FLAG_n (FORCED_FLAG<<3)
#endif

#ifdef FOR_openvt
#ifndef TT
#define TT this.openvt
#endif
#define FLAG_w (FORCED_FLAG<<0)
#define FLAG_s (FORCED_FLAG<<1)
#define FLAG_c (FORCED_FLAG<<2)
#endif

#ifdef FOR_partprobe
#ifndef TT
#define TT this.partprobe
#endif
#endif

#ifdef FOR_passwd
#ifndef TT
#define TT this.passwd
#endif
#define FLAG_u (FORCED_FLAG<<0)
#define FLAG_l (FORCED_FLAG<<1)
#define FLAG_d (FORCED_FLAG<<2)
#define FLAG_a (FORCED_FLAG<<3)
#endif

#ifdef FOR_paste
#ifndef TT
#define TT this.paste
#endif
#define FLAG_s (1<<0)
#define FLAG_d (1<<1)
#endif

#ifdef FOR_patch
#ifndef TT
#define TT this.patch
#endif
#define FLAG_s (1<<0)
#define FLAG_R (1<<1)
#define FLAG_i (1<<2)
#define FLAG_d (1<<3)
#define FLAG_p (1<<4)
#define FLAG_l (1<<5)
#define FLAG_u (1<<6)
#define FLAG_f (1<<7)
#define FLAG_g (1<<8)
#define FLAG_F (1<<9)
#define FLAG_x (FORCED_FLAG<<10)
#define FLAG_dry_run (1<<11)
#define FLAG_no_backup_if_mismatch (1<<12)
#endif

#ifdef FOR_pgrep
#ifndef TT
#define TT this.pgrep
#endif
#define FLAG_L (FORCED_FLAG<<0)
#define FLAG_x (FORCED_FLAG<<1)
#define FLAG_v (FORCED_FLAG<<2)
#define FLAG_o (FORCED_FLAG<<3)
#define FLAG_n (FORCED_FLAG<<4)
#define FLAG_f (FORCED_FLAG<<5)
#define FLAG_G (FORCED_FLAG<<6)
#define FLAG_g (FORCED_FLAG<<7)
#define FLAG_P (FORCED_FLAG<<8)
#define FLAG_s (FORCED_FLAG<<9)
#define FLAG_t (FORCED_FLAG<<10)
#define FLAG_U (FORCED_FLAG<<11)
#define FLAG_u (FORCED_FLAG<<12)
#define FLAG_d (FORCED_FLAG<<13)
#define FLAG_l (FORCED_FLAG<<14)
#define FLAG_c (FORCED_FLAG<<15)
#endif

#ifdef FOR_pidof
#ifndef TT
#define TT this.pidof
#endif
#define FLAG_x (FORCED_FLAG<<0)
#define FLAG_o (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#endif

#ifdef FOR_ping
#ifndef TT
#define TT this.ping
#endif
#define FLAG_I (FORCED_FLAG<<0)
#define FLAG_6 (FORCED_FLAG<<1)
#define FLAG_4 (FORCED_FLAG<<2)
#define FLAG_f (FORCED_FLAG<<3)
#define FLAG_q (FORCED_FLAG<<4)
#define FLAG_w (FORCED_FLAG<<5)
#define FLAG_W (FORCED_FLAG<<6)
#define FLAG_i (FORCED_FLAG<<7)
#define FLAG_s (FORCED_FLAG<<8)
#define FLAG_c (FORCED_FLAG<<9)
#define FLAG_t (FORCED_FLAG<<10)
#define FLAG_m (FORCED_FLAG<<11)
#endif

#ifdef FOR_pivot_root
#ifndef TT
#define TT this.pivot_root
#endif
#endif

#ifdef FOR_pkill
#ifndef TT
#define TT this.pkill
#endif
#define FLAG_l (FORCED_FLAG<<0)
#define FLAG_x (FORCED_FLAG<<1)
#define FLAG_v (FORCED_FLAG<<2)
#define FLAG_o (FORCED_FLAG<<3)
#define FLAG_n (FORCED_FLAG<<4)
#define FLAG_f (FORCED_FLAG<<5)
#define FLAG_G (FORCED_FLAG<<6)
#define FLAG_g (FORCED_FLAG<<7)
#define FLAG_P (FORCED_FLAG<<8)
#define FLAG_s (FORCED_FLAG<<9)
#define FLAG_t (FORCED_FLAG<<10)
#define FLAG_U (FORCED_FLAG<<11)
#define FLAG_u (FORCED_FLAG<<12)
#define FLAG_V (FORCED_FLAG<<13)
#endif

#ifdef FOR_pmap
#ifndef TT
#define TT this.pmap
#endif
#define FLAG_q (FORCED_FLAG<<0)
#define FLAG_x (FORCED_FLAG<<1)
#endif

#ifdef FOR_printenv
#ifndef TT
#define TT this.printenv
#endif
#define FLAG_0 (FORCED_FLAG<<0)
#endif

#ifdef FOR_printf
#ifndef TT
#define TT this.printf
#endif
#endif

#ifdef FOR_ps
#ifndef TT
#define TT this.ps
#endif
#define FLAG_Z (FORCED_FLAG<<0)
#define FLAG_w (FORCED_FLAG<<1)
#define FLAG_G (FORCED_FLAG<<2)
#define FLAG_g (FORCED_FLAG<<3)
#define FLAG_U (FORCED_FLAG<<4)
#define FLAG_u (FORCED_FLAG<<5)
#define FLAG_T (FORCED_FLAG<<6)
#define FLAG_t (FORCED_FLAG<<7)
#define FLAG_s (FORCED_FLAG<<8)
#define FLAG_p (FORCED_FLAG<<9)
#define FLAG_O (FORCED_FLAG<<10)
#define FLAG_o (FORCED_FLAG<<11)
#define FLAG_n (FORCED_FLAG<<12)
#define FLAG_M (FORCED_FLAG<<13)
#define FLAG_l (FORCED_FLAG<<14)
#define FLAG_f (FORCED_FLAG<<15)
#define FLAG_e (FORCED_FLAG<<16)
#define FLAG_d (FORCED_FLAG<<17)
#define FLAG_A (FORCED_FLAG<<18)
#define FLAG_a (FORCED_FLAG<<19)
#define FLAG_P (FORCED_FLAG<<20)
#define FLAG_k (FORCED_FLAG<<21)
#endif

#ifdef FOR_pwd
#ifndef TT
#define TT this.pwd
#endif
#define FLAG_P (1<<0)
#define FLAG_L (1<<1)
#endif

#ifdef FOR_pwdx
#ifndef TT
#define TT this.pwdx
#endif
#define FLAG_a (FORCED_FLAG<<0)
#endif

#ifdef FOR_readahead
#ifndef TT
#define TT this.readahead
#endif
#endif

#ifdef FOR_readelf
#ifndef TT
#define TT this.readelf
#endif
#define FLAG_x (FORCED_FLAG<<0)
#define FLAG_W (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#define FLAG_S (FORCED_FLAG<<3)
#define FLAG_p (FORCED_FLAG<<4)
#define FLAG_n (FORCED_FLAG<<5)
#define FLAG_l (FORCED_FLAG<<6)
#define FLAG_h (FORCED_FLAG<<7)
#define FLAG_d (FORCED_FLAG<<8)
#define FLAG_a (FORCED_FLAG<<9)
#define FLAG_dyn_syms (FORCED_FLAG<<10)
#endif

#ifdef FOR_readlink
#ifndef TT
#define TT this.readlink
#endif
#define FLAG_f (1<<0)
#define FLAG_e (1<<1)
#define FLAG_m (1<<2)
#define FLAG_q (1<<3)
#define FLAG_n (1<<4)
#endif

#ifdef FOR_realpath
#ifndef TT
#define TT this.realpath
#endif
#endif

#ifdef FOR_reboot
#ifndef TT
#define TT this.reboot
#endif
#define FLAG_n (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#endif

#ifdef FOR_renice
#ifndef TT
#define TT this.renice
#endif
#define FLAG_n (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_p (FORCED_FLAG<<2)
#define FLAG_g (FORCED_FLAG<<3)
#endif

#ifdef FOR_reset
#ifndef TT
#define TT this.reset
#endif
#endif

#ifdef FOR_restorecon
#ifndef TT
#define TT this.restorecon
#endif
#define FLAG_v (FORCED_FLAG<<0)
#define FLAG_r (FORCED_FLAG<<1)
#define FLAG_R (FORCED_FLAG<<2)
#define FLAG_n (FORCED_FLAG<<3)
#define FLAG_F (FORCED_FLAG<<4)
#define FLAG_D (FORCED_FLAG<<5)
#endif

#ifdef FOR_rev
#ifndef TT
#define TT this.rev
#endif
#endif

#ifdef FOR_rfkill
#ifndef TT
#define TT this.rfkill
#endif
#endif

#ifdef FOR_rm
#ifndef TT
#define TT this.rm
#endif
#define FLAG_v (1<<0)
#define FLAG_r (1<<1)
#define FLAG_R (1<<2)
#define FLAG_i (1<<3)
#define FLAG_f (1<<4)
#endif

#ifdef FOR_rmdir
#ifndef TT
#define TT this.rmdir
#endif
#define FLAG_p (1<<0)
#define FLAG_ignore_fail_on_non_empty (1<<1)
#endif

#ifdef FOR_rmmod
#ifndef TT
#define TT this.rmmod
#endif
#define FLAG_f (FORCED_FLAG<<0)
#define FLAG_w (FORCED_FLAG<<1)
#endif

#ifdef FOR_route
#ifndef TT
#define TT this.route
#endif
#define FLAG_A (FORCED_FLAG<<0)
#define FLAG_e (FORCED_FLAG<<1)
#define FLAG_n (FORCED_FLAG<<2)
#endif

#ifdef FOR_runcon
#ifndef TT
#define TT this.runcon
#endif
#endif

#ifdef FOR_sed
#ifndef TT
#define TT this.sed
#endif
#define FLAG_z (1<<0)
#define FLAG_r (1<<1)
#define FLAG_E (1<<2)
#define FLAG_n (1<<3)
#define FLAG_i (1<<4)
#define FLAG_f (1<<5)
#define FLAG_e (1<<6)
#define FLAG_version (1<<7)
#define FLAG_help (1<<8)
#endif

#ifdef FOR_sendevent
#ifndef TT
#define TT this.sendevent
#endif
#endif

#ifdef FOR_seq
#ifndef TT
#define TT this.seq
#endif
#define FLAG_w (1<<0)
#define FLAG_s (1<<1)
#define FLAG_f (1<<2)
#endif

#ifdef FOR_setenforce
#ifndef TT
#define TT this.setenforce
#endif
#endif

#ifdef FOR_setfattr
#ifndef TT
#define TT this.setfattr
#endif
#define FLAG_x (FORCED_FLAG<<0)
#define FLAG_v (FORCED_FLAG<<1)
#define FLAG_n (FORCED_FLAG<<2)
#define FLAG_h (FORCED_FLAG<<3)
#endif

#ifdef FOR_setsid
#ifndef TT
#define TT this.setsid
#endif
#define FLAG_d (1<<0)
#define FLAG_c (1<<1)
#define FLAG_w (1<<2)
#endif

#ifdef FOR_sh
#ifndef TT
#define TT this.sh
#endif
#define FLAG_i (FORCED_FLAG<<0)
#define FLAG_c (FORCED_FLAG<<1)
#define FLAG_s (FORCED_FLAG<<2)
#define FLAG_norc (FORCED_FLAG<<3)
#define FLAG_noprofile (FORCED_FLAG<<4)
#define FLAG_noediting (FORCED_FLAG<<5)
#endif

#ifdef FOR_sha1sum
#ifndef TT
#define TT this.sha1sum
#endif
#define FLAG_s (1<<0)
#define FLAG_c (1<<1)
#define FLAG_b (1<<2)
#endif

#ifdef FOR_shred
#ifndef TT
#define TT this.shred
#endif
#define FLAG_f (FORCED_FLAG<<0)
#define FLAG_o (FORCED_FLAG<<1)
#define FLAG_n (FORCED_FLAG<<2)
#define FLAG_s (FORCED_FLAG<<3)
#define FLAG_u (FORCED_FLAG<<4)
#define FLAG_x (FORCED_FLAG<<5)
#define FLAG_z (FORCED_FLAG<<6)
#endif

#ifdef FOR_skeleton
#ifndef TT
#define TT this.skeleton
#endif
#define FLAG_a (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_c (FORCED_FLAG<<2)
#define FLAG_d (FORCED_FLAG<<3)
#define FLAG_e (FORCED_FLAG<<4)
#define FLAG_also (FORCED_FLAG<<5)
#define FLAG_blubber (FORCED_FLAG<<6)
#define FLAG_walrus (FORCED_FLAG<<7)
#endif

#ifdef FOR_skeleton_alias
#ifndef TT
#define TT this.skeleton_alias
#endif
#define FLAG_q (FORCED_FLAG<<0)
#define FLAG_d (FORCED_FLAG<<1)
#define FLAG_b (FORCED_FLAG<<2)
#endif

#ifdef FOR_sleep
#ifndef TT
#define TT this.sleep
#endif
#endif

#ifdef FOR_sntp
#ifndef TT
#define TT this.sntp
#endif
#define FLAG_r (FORCED_FLAG<<0)
#define FLAG_q (FORCED_FLAG<<1)
#define FLAG_D (FORCED_FLAG<<2)
#define FLAG_d (FORCED_FLAG<<3)
#define FLAG_s (FORCED_FLAG<<4)
#define FLAG_a (FORCED_FLAG<<5)
#define FLAG_t (FORCED_FLAG<<6)
#define FLAG_p (FORCED_FLAG<<7)
#define FLAG_S (FORCED_FLAG<<8)
#define FLAG_m (FORCED_FLAG<<9)
#define FLAG_M (FORCED_FLAG<<10)
#endif

#ifdef FOR_sort
#ifndef TT
#define TT this.sort
#endif
#define FLAG_n (1<<0)
#define FLAG_u (1<<1)
#define FLAG_r (1<<2)
#define FLAG_i (1<<3)
#define FLAG_f (1<<4)
#define FLAG_d (1<<5)
#define FLAG_z (1<<6)
#define FLAG_s (1<<7)
#define FLAG_c (1<<8)
#define FLAG_M (1<<9)
#define FLAG_b (1<<10)
#define FLAG_V (1<<11)
#define FLAG_x (1<<12)
#define FLAG_t (1<<13)
#define FLAG_k (1<<14)
#define FLAG_o (1<<15)
#define FLAG_m (1<<16)
#define FLAG_T (1<<17)
#define FLAG_S (1<<18)
#define FLAG_g (1<<19)
#endif

#ifdef FOR_split
#ifndef TT
#define TT this.split
#endif
#define FLAG_l (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_a (FORCED_FLAG<<2)
#endif

#ifdef FOR_stat
#ifndef TT
#define TT this.stat
#endif
#define FLAG_t (1<<0)
#define FLAG_L (1<<1)
#define FLAG_f (1<<2)
#define FLAG_c (1<<3)
#endif

#ifdef FOR_strings
#ifndef TT
#define TT this.strings
#endif
#define FLAG_o (FORCED_FLAG<<0)
#define FLAG_f (FORCED_FLAG<<1)
#define FLAG_n (FORCED_FLAG<<2)
#define FLAG_a (FORCED_FLAG<<3)
#define FLAG_t (FORCED_FLAG<<4)
#endif

#ifdef FOR_stty
#ifndef TT
#define TT this.stty
#endif
#define FLAG_g (FORCED_FLAG<<0)
#define FLAG_F (FORCED_FLAG<<1)
#define FLAG_a (FORCED_FLAG<<2)
#endif

#ifdef FOR_su
#ifndef TT
#define TT this.su
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_c (FORCED_FLAG<<1)
#define FLAG_g (FORCED_FLAG<<2)
#define FLAG_u (FORCED_FLAG<<3)
#define FLAG_p (FORCED_FLAG<<4)
#define FLAG_m (FORCED_FLAG<<5)
#define FLAG_l (FORCED_FLAG<<6)
#endif

#ifdef FOR_sulogin
#ifndef TT
#define TT this.sulogin
#endif
#define FLAG_t (FORCED_FLAG<<0)
#endif

#ifdef FOR_swapoff
#ifndef TT
#define TT this.swapoff
#endif
#endif

#ifdef FOR_swapon
#ifndef TT
#define TT this.swapon
#endif
#define FLAG_d (FORCED_FLAG<<0)
#define FLAG_p (FORCED_FLAG<<1)
#endif

#ifdef FOR_switch_root
#ifndef TT
#define TT this.switch_root
#endif
#define FLAG_h (FORCED_FLAG<<0)
#define FLAG_c (FORCED_FLAG<<1)
#endif

#ifdef FOR_sync
#ifndef TT
#define TT this.sync
#endif
#endif

#ifdef FOR_sysctl
#ifndef TT
#define TT this.sysctl
#endif
#define FLAG_A (FORCED_FLAG<<0)
#define FLAG_a (FORCED_FLAG<<1)
#define FLAG_p (FORCED_FLAG<<2)
#define FLAG_w (FORCED_FLAG<<3)
#define FLAG_q (FORCED_FLAG<<4)
#define FLAG_N (FORCED_FLAG<<5)
#define FLAG_e (FORCED_FLAG<<6)
#define FLAG_n (FORCED_FLAG<<7)
#endif

#ifdef FOR_syslogd
#ifndef TT
#define TT this.syslogd
#endif
#define FLAG_D (FORCED_FLAG<<0)
#define FLAG_L (FORCED_FLAG<<1)
#define FLAG_K (FORCED_FLAG<<2)
#define FLAG_S (FORCED_FLAG<<3)
#define FLAG_n (FORCED_FLAG<<4)
#define FLAG_a (FORCED_FLAG<<5)
#define FLAG_f (FORCED_FLAG<<6)
#define FLAG_p (FORCED_FLAG<<7)
#define FLAG_O (FORCED_FLAG<<8)
#define FLAG_m (FORCED_FLAG<<9)
#define FLAG_s (FORCED_FLAG<<10)
#define FLAG_b (FORCED_FLAG<<11)
#define FLAG_R (FORCED_FLAG<<12)
#define FLAG_l (FORCED_FLAG<<13)
#endif

#ifdef FOR_tac
#ifndef TT
#define TT this.tac
#endif
#endif

#ifdef FOR_tail
#ifndef TT
#define TT this.tail
#endif
#define FLAG_n (1<<0)
#define FLAG_c (1<<1)
#define FLAG_f (1<<2)
#endif

#ifdef FOR_tar
#ifndef TT
#define TT this.tar
#endif
#define FLAG_a (1<<0)
#define FLAG_f (1<<1)
#define FLAG_C (1<<2)
#define FLAG_T (1<<3)
#define FLAG_X (1<<4)
#define FLAG_m (1<<5)
#define FLAG_O (1<<6)
#define FLAG_S (1<<7)
#define FLAG_z (1<<8)
#define FLAG_j (1<<9)
#define FLAG_J (1<<10)
#define FLAG_v (1<<11)
#define FLAG_t (1<<12)
#define FLAG_x (1<<13)
#define FLAG_h (1<<14)
#define FLAG_c (1<<15)
#define FLAG_k (1<<16)
#define FLAG_p (1<<17)
#define FLAG_o (1<<18)
#define FLAG_to_command (1<<19)
#define FLAG_owner (1<<20)
#define FLAG_group (1<<21)
#define FLAG_mtime (1<<22)
#define FLAG_mode (1<<23)
#define FLAG_exclude (1<<24)
#define FLAG_overwrite (1<<25)
#define FLAG_no_same_permissions (1<<26)
#define FLAG_numeric_owner (1<<27)
#define FLAG_no_recursion (1<<28)
#define FLAG_full_time (1<<29)
#define FLAG_restrict (1<<30)
#endif

#ifdef FOR_taskset
#ifndef TT
#define TT this.taskset
#endif
#define FLAG_a (FORCED_FLAG<<0)
#define FLAG_p (FORCED_FLAG<<1)
#endif

#ifdef FOR_tcpsvd
#ifndef TT
#define TT this.tcpsvd
#endif
#define FLAG_v (FORCED_FLAG<<0)
#define FLAG_E (FORCED_FLAG<<1)
#define FLAG_h (FORCED_FLAG<<2)
#define FLAG_l (FORCED_FLAG<<3)
#define FLAG_u (FORCED_FLAG<<4)
#define FLAG_b (FORCED_FLAG<<5)
#define FLAG_C (FORCED_FLAG<<6)
#define FLAG_c (FORCED_FLAG<<7)
#endif

#ifdef FOR_tee
#ifndef TT
#define TT this.tee
#endif
#define FLAG_a (1<<0)
#define FLAG_i (1<<1)
#endif

#ifdef FOR_telnet
#ifndef TT
#define TT this.telnet
#endif
#endif

#ifdef FOR_telnetd
#ifndef TT
#define TT this.telnetd
#endif
#define FLAG_i (FORCED_FLAG<<0)
#define FLAG_K (FORCED_FLAG<<1)
#define FLAG_S (FORCED_FLAG<<2)
#define FLAG_F (FORCED_FLAG<<3)
#define FLAG_l (FORCED_FLAG<<4)
#define FLAG_f (FORCED_FLAG<<5)
#define FLAG_p (FORCED_FLAG<<6)
#define FLAG_b (FORCED_FLAG<<7)
#define FLAG_w (FORCED_FLAG<<8)
#endif

#ifdef FOR_test
#ifndef TT
#define TT this.test
#endif
#endif

#ifdef FOR_tftp
#ifndef TT
#define TT this.tftp
#endif
#define FLAG_p (FORCED_FLAG<<0)
#define FLAG_g (FORCED_FLAG<<1)
#define FLAG_l (FORCED_FLAG<<2)
#define FLAG_r (FORCED_FLAG<<3)
#define FLAG_b (FORCED_FLAG<<4)
#endif

#ifdef FOR_tftpd
#ifndef TT
#define TT this.tftpd
#endif
#define FLAG_l (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_c (FORCED_FLAG<<2)
#define FLAG_r (FORCED_FLAG<<3)
#endif

#ifdef FOR_time
#ifndef TT
#define TT this.time
#endif
#define FLAG_v (FORCED_FLAG<<0)
#define FLAG_p (FORCED_FLAG<<1)
#endif

#ifdef FOR_timeout
#ifndef TT
#define TT this.timeout
#endif
#define FLAG_s (1<<0)
#define FLAG_k (1<<1)
#define FLAG_v (1<<2)
#define FLAG_preserve_status (1<<3)
#define FLAG_foreground (1<<4)
#endif

#ifdef FOR_top
#ifndef TT
#define TT this.top
#endif
#define FLAG_q (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_n (FORCED_FLAG<<2)
#define FLAG_m (FORCED_FLAG<<3)
#define FLAG_d (FORCED_FLAG<<4)
#define FLAG_s (FORCED_FLAG<<5)
#define FLAG_u (FORCED_FLAG<<6)
#define FLAG_p (FORCED_FLAG<<7)
#define FLAG_o (FORCED_FLAG<<8)
#define FLAG_k (FORCED_FLAG<<9)
#define FLAG_H (FORCED_FLAG<<10)
#define FLAG_O (FORCED_FLAG<<11)
#endif

#ifdef FOR_touch
#ifndef TT
#define TT this.touch
#endif
#define FLAG_h (1<<0)
#define FLAG_t (1<<1)
#define FLAG_r (1<<2)
#define FLAG_m (1<<3)
#define FLAG_f (1<<4)
#define FLAG_d (1<<5)
#define FLAG_c (1<<6)
#define FLAG_a (1<<7)
#endif

#ifdef FOR_toybox
#ifndef TT
#define TT this.toybox
#endif
#endif

#ifdef FOR_tr
#ifndef TT
#define TT this.tr
#endif
#define FLAG_d (1<<0)
#define FLAG_s (1<<1)
#define FLAG_c (1<<2)
#define FLAG_C (1<<3)
#endif

#ifdef FOR_traceroute
#ifndef TT
#define TT this.traceroute
#endif
#define FLAG_4 (FORCED_FLAG<<0)
#define FLAG_6 (FORCED_FLAG<<1)
#define FLAG_F (FORCED_FLAG<<2)
#define FLAG_U (FORCED_FLAG<<3)
#define FLAG_I (FORCED_FLAG<<4)
#define FLAG_l (FORCED_FLAG<<5)
#define FLAG_d (FORCED_FLAG<<6)
#define FLAG_n (FORCED_FLAG<<7)
#define FLAG_v (FORCED_FLAG<<8)
#define FLAG_r (FORCED_FLAG<<9)
#define FLAG_m (FORCED_FLAG<<10)
#define FLAG_p (FORCED_FLAG<<11)
#define FLAG_q (FORCED_FLAG<<12)
#define FLAG_s (FORCED_FLAG<<13)
#define FLAG_t (FORCED_FLAG<<14)
#define FLAG_w (FORCED_FLAG<<15)
#define FLAG_g (FORCED_FLAG<<16)
#define FLAG_z (FORCED_FLAG<<17)
#define FLAG_f (FORCED_FLAG<<18)
#define FLAG_i (FORCED_FLAG<<19)
#endif

#ifdef FOR_true
#ifndef TT
#define TT this.true
#endif
#endif

#ifdef FOR_truncate
#ifndef TT
#define TT this.truncate
#endif
#define FLAG_c (1<<0)
#define FLAG_s (1<<1)
#endif

#ifdef FOR_tty
#ifndef TT
#define TT this.tty
#endif
#define FLAG_s (FORCED_FLAG<<0)
#endif

#ifdef FOR_tunctl
#ifndef TT
#define TT this.tunctl
#endif
#define FLAG_T (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_d (FORCED_FLAG<<2)
#define FLAG_t (FORCED_FLAG<<3)
#endif

#ifdef FOR_ulimit
#ifndef TT
#define TT this.ulimit
#endif
#define FLAG_c (FORCED_FLAG<<0)
#define FLAG_d (FORCED_FLAG<<1)
#define FLAG_e (FORCED_FLAG<<2)
#define FLAG_f (FORCED_FLAG<<3)
#define FLAG_i (FORCED_FLAG<<4)
#define FLAG_l (FORCED_FLAG<<5)
#define FLAG_m (FORCED_FLAG<<6)
#define FLAG_n (FORCED_FLAG<<7)
#define FLAG_p (FORCED_FLAG<<8)
#define FLAG_q (FORCED_FLAG<<9)
#define FLAG_R (FORCED_FLAG<<10)
#define FLAG_r (FORCED_FLAG<<11)
#define FLAG_s (FORCED_FLAG<<12)
#define FLAG_t (FORCED_FLAG<<13)
#define FLAG_u (FORCED_FLAG<<14)
#define FLAG_v (FORCED_FLAG<<15)
#define FLAG_a (FORCED_FLAG<<16)
#define FLAG_H (FORCED_FLAG<<17)
#define FLAG_S (FORCED_FLAG<<18)
#define FLAG_P (FORCED_FLAG<<19)
#endif

#ifdef FOR_umount
#ifndef TT
#define TT this.umount
#endif
#define FLAG_v (FORCED_FLAG<<0)
#define FLAG_t (FORCED_FLAG<<1)
#define FLAG_a (FORCED_FLAG<<2)
#define FLAG_r (FORCED_FLAG<<3)
#define FLAG_l (FORCED_FLAG<<4)
#define FLAG_f (FORCED_FLAG<<5)
#define FLAG_D (FORCED_FLAG<<6)
#define FLAG_d (FORCED_FLAG<<7)
#define FLAG_n (FORCED_FLAG<<8)
#define FLAG_c (FORCED_FLAG<<9)
#endif

#ifdef FOR_uname
#ifndef TT
#define TT this.uname
#endif
#define FLAG_s (1<<0)
#define FLAG_n (1<<1)
#define FLAG_r (1<<2)
#define FLAG_v (1<<3)
#define FLAG_m (1<<4)
#define FLAG_a (1<<5)
#define FLAG_o (1<<6)
#endif

#ifdef FOR_uniq
#ifndef TT
#define TT this.uniq
#endif
#define FLAG_u (1<<0)
#define FLAG_d (1<<1)
#define FLAG_c (1<<2)
#define FLAG_i (1<<3)
#define FLAG_z (1<<4)
#define FLAG_w (1<<5)
#define FLAG_s (1<<6)
#define FLAG_f (1<<7)
#endif

#ifdef FOR_unix2dos
#ifndef TT
#define TT this.unix2dos
#endif
#endif

#ifdef FOR_unlink
#ifndef TT
#define TT this.unlink
#endif
#endif

#ifdef FOR_unshare
#ifndef TT
#define TT this.unshare
#endif
#define FLAG_U (FORCED_FLAG<<0)
#define FLAG_u (FORCED_FLAG<<1)
#define FLAG_p (FORCED_FLAG<<2)
#define FLAG_n (FORCED_FLAG<<3)
#define FLAG_m (FORCED_FLAG<<4)
#define FLAG_i (FORCED_FLAG<<5)
#define FLAG_r (FORCED_FLAG<<6)
#define FLAG_f (FORCED_FLAG<<7)
#endif

#ifdef FOR_uptime
#ifndef TT
#define TT this.uptime
#endif
#define FLAG_s (FORCED_FLAG<<0)
#define FLAG_p (FORCED_FLAG<<1)
#endif

#ifdef FOR_useradd
#ifndef TT
#define TT this.useradd
#endif
#define FLAG_H (FORCED_FLAG<<0)
#define FLAG_D (FORCED_FLAG<<1)
#define FLAG_S (FORCED_FLAG<<2)
#define FLAG_h (FORCED_FLAG<<3)
#define FLAG_g (FORCED_FLAG<<4)
#define FLAG_s (FORCED_FLAG<<5)
#define FLAG_G (FORCED_FLAG<<6)
#define FLAG_u (FORCED_FLAG<<7)
#endif

#ifdef FOR_userdel
#ifndef TT
#define TT this.userdel
#endif
#define FLAG_r (FORCED_FLAG<<0)
#endif

#ifdef FOR_usleep
#ifndef TT
#define TT this.usleep
#endif
#endif

#ifdef FOR_uudecode
#ifndef TT
#define TT this.uudecode
#endif
#define FLAG_o (FORCED_FLAG<<0)
#endif

#ifdef FOR_uuencode
#ifndef TT
#define TT this.uuencode
#endif
#define FLAG_m (FORCED_FLAG<<0)
#endif

#ifdef FOR_uuidgen
#ifndef TT
#define TT this.uuidgen
#endif
#define FLAG_r (FORCED_FLAG<<0)
#endif

#ifdef FOR_vconfig
#ifndef TT
#define TT this.vconfig
#endif
#endif

#ifdef FOR_vi
#ifndef TT
#define TT this.vi
#endif
#define FLAG_s (FORCED_FLAG<<0)
#endif

#ifdef FOR_vmstat
#ifndef TT
#define TT this.vmstat
#endif
#define FLAG_n (FORCED_FLAG<<0)
#endif

#ifdef FOR_w
#ifndef TT
#define TT this.w
#endif
#endif

#ifdef FOR_watch
#ifndef TT
#define TT this.watch
#endif
#define FLAG_x (FORCED_FLAG<<0)
#define FLAG_b (FORCED_FLAG<<1)
#define FLAG_e (FORCED_FLAG<<2)
#define FLAG_t (FORCED_FLAG<<3)
#define FLAG_n (FORCED_FLAG<<4)
#endif

#ifdef FOR_wc
#ifndef TT
#define TT this.wc
#endif
#define FLAG_l (1<<0)
#define FLAG_w (1<<1)
#define FLAG_c (1<<2)
#define FLAG_m (1<<3)
#endif

#ifdef FOR_wget
#ifndef TT
#define TT this.wget
#endif
#define FLAG_O (FORCED_FLAG<<0)
#define FLAG_no_check_certificate (FORCED_FLAG<<1)
#endif

#ifdef FOR_which
#ifndef TT
#define TT this.which
#endif
#define FLAG_a (1<<0)
#endif

#ifdef FOR_who
#ifndef TT
#define TT this.who
#endif
#define FLAG_a (FORCED_FLAG<<0)
#endif

#ifdef FOR_xargs
#ifndef TT
#define TT this.xargs
#endif
#define FLAG_0 (1<<0)
#define FLAG_s (1<<1)
#define FLAG_n (1<<2)
#define FLAG_r (1<<3)
#define FLAG_t (1<<4)
#define FLAG_p (1<<5)
#define FLAG_o (1<<6)
#define FLAG_P (1<<7)
#define FLAG_E (1<<8)
#endif

#ifdef FOR_xxd
#ifndef TT
#define TT this.xxd
#endif
#define FLAG_s (1<<0)
#define FLAG_r (1<<1)
#define FLAG_p (1<<2)
#define FLAG_i (1<<3)
#define FLAG_g (1<<4)
#define FLAG_o (1<<5)
#define FLAG_l (1<<6)
#define FLAG_c (1<<7)
#endif

#ifdef FOR_xzcat
#ifndef TT
#define TT this.xzcat
#endif
#endif

#ifdef FOR_yes
#ifndef TT
#define TT this.yes
#endif
#endif

#ifdef FOR_zcat
#ifndef TT
#define TT this.zcat
#endif
#define FLAG_9 (1<<0)
#define FLAG_8 (1<<1)
#define FLAG_7 (1<<2)
#define FLAG_6 (1<<3)
#define FLAG_5 (1<<4)
#define FLAG_4 (1<<5)
#define FLAG_3 (1<<6)
#define FLAG_2 (1<<7)
#define FLAG_1 (1<<8)
#define FLAG_k (1<<9)
#define FLAG_f (1<<10)
#define FLAG_d (1<<11)
#define FLAG_c (1<<12)
#endif

