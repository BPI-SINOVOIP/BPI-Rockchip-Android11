
testdata_progs = [ 'successtest',
		   'no-subtests',
		   'skippers',
		 ]

testdata_executables = []

foreach prog : testdata_progs
	testdata_executables += executable(prog, prog + '.c',
					   dependencies : igt_deps,
					   install : false)
endforeach

configure_file(input : 'test-blacklist.txt',
	       output : 'test-blacklist.txt', copy : true)
configure_file(input : 'test-blacklist2.txt',
	       output : 'test-blacklist2.txt', copy : true)

testdata_list = custom_target('testdata_testlist',
			      output : 'test-list.txt',
			      command : [ gen_testlist, '@OUTPUT@', testdata_progs ],
			      build_by_default : true)

testdata_dir = meson.current_build_dir()
