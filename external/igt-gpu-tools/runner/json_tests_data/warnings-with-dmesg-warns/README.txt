A test that has output in stderr should result in 'warn' instead of
'pass'. But if there is a message in the kernel logs that would make
the result 'dmesg-warn' instead, that takes priority over 'warn'.
