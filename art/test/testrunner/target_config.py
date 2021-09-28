target_config = {

    # Configuration syntax:
    #
    #   Required keys: (Use one or more of these)
    #    * golem - specify a golem machine-type to build, e.g. android-armv8
    #              (uses art/tools/golem/build-target.sh)
    #    * make - specify a make target to build, e.g. build-art-host
    #    * run-test - runs the tests in art/test/ directory with testrunner.py,
    #                 specify a list of arguments to pass to testrunner.py
    #
    #   Optional keys: (Use any of these)
    #    * env - Add additional environment variable to the current environment.
    #
    # *** IMPORTANT ***:
    #    This configuration is used by the android build server. Targets must not be renamed
    #    or removed.
    #

    ##########################################

    # General ART configurations.
    # Calls make and testrunner both.

    'art-test' : {
        'make' : 'test-art-host-gtest',
        'run-test' : []
    },

    'art-test-javac' : {
        'run-test' : ['--jvm']
    },

    # ART run-test configurations
    # (calls testrunner which builds and then runs the test targets)

    'art-ndebug' : {
        'run-test' : ['--ndebug']
    },
    'art-interpreter' : {
        'run-test' : ['--interpreter']
    },
    'art-interpreter-cxx' : {
        'run-test' : ['--interpreter'],
        'env' : {
            'ART_USE_CXX_INTERPRETER' : 'true'
        }
    },
    'art-interpreter-access-checks' : {
        'run-test' : ['--interp-ac']
    },
    'art-jit' : {
        'run-test' : ['--jit', '--debuggable', '--ndebuggable']
    },
    'art-jit-on-first-use' : {
        'run-test' : ['--jit-on-first-use']
    },
    'art-pictest' : {
        # Deprecated config: All AOT-compiled code is PIC now.
        'run-test' : ['--optimizing']
    },
    'art-gcstress-gcverify': {
        # Do not exercise '--interpreter', '--optimizing', nor '--jit' in this
        # configuration, as they are covered by the
        # 'art-interpreter-gcstress-gcverify',
        # 'art-optimizing-gcstress-gcverify' and 'art-jit-gcstress-gcverify'
        # configurations below.
        'run-test': ['--interp-ac',
                     '--speed-profile',
                     '--gcstress',
                     '--gcverify']
    },
    'art-interpreter-gcstress-gcverify' : {
        'run-test' : ['--interpreter',
                      '--gcstress',
                      '--gcverify']
    },
    'art-optimizing-gcstress-gcverify' : {
        'run-test' : ['--optimizing',
                      '--gcstress',
                      '--gcverify']
    },
    'art-jit-gcstress-gcverify' : {
        'run-test' : ['--jit',
                      '--gcstress',
                      '--gcverify']
    },
    'art-jit-on-first-use-gcstress' : {
        'run-test' : ['--jit-on-first-use',
                      '--gcstress']
    },
    'art-read-barrier-heap-poisoning' : {
        'run-test': ['--interpreter',
                     '--optimizing'],
        'env' : {
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-read-barrier-table-lookup' : {
        'run-test' : ['--interpreter',
                      '--optimizing'],
        'env' : {
            'ART_READ_BARRIER_TYPE' : 'TABLELOOKUP',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-debug-gc' : {
        'run-test' : ['--interpreter',
                      '--optimizing'],
        'env' : {
            'ART_TEST_DEBUG_GC' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    # TODO: Consider removing this configuration when it is no longer used by
    # any continuous testing target (b/62611253), as the SS collector overlaps
    # with the CC collector, since both move objects.
    'art-ss-gc' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    # TODO: Consider removing this configuration when it is no longer used by
    # any continuous testing target (b/62611253), as the SS collector overlaps
    # with the CC collector, since both move objects.
    'art-ss-gc-tlab' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-tracing' : {
        'run-test' : ['--trace']
    },
    'art-interpreter-tracing' : {
        'run-test' : ['--interpreter',
                      '--trace']
    },
    'art-forcecopy' : {
        'run-test' : ['--forcecopy']
    },
    'art-no-prebuild' : {
        'run-test' : ['--no-prebuild']
    },
    'art-no-image' : {
        'run-test' : ['--no-image']
    },
    'art-interpreter-no-image' : {
        'run-test' : ['--interpreter',
                      '--no-image']
    },
    'art-heap-poisoning' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--cdex-none'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_HEAP_POISONING' : 'true',
            # Disable compact dex to get coverage of standard dex file usage.
            'ART_DEFAULT_COMPACT_DEX_LEVEL' : 'none'
        }
    },
    'art-preopt' : {
        # This test configuration is intended to be representative of the case
        # of preopted apps, which are precompiled against an
        # unrelocated image, then used with a relocated image.
        'run-test' : ['--prebuild',
                      '--relocate',
                      '--jit']
    },

    # ART gtest configurations
    # (calls make 'target' which builds and then runs the gtests).

    'art-gtest' : {
        'make' :  'test-art-host-gtest'
    },
    'art-gtest-read-barrier': {
        'make' :  'test-art-host-gtest',
        'env' : {
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-gtest-read-barrier-table-lookup': {
        'make' :  'test-art-host-gtest',
        'env': {
            'ART_READ_BARRIER_TYPE' : 'TABLELOOKUP',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    # TODO: Consider removing this configuration when it is no longer used by
    # any continuous testing target (b/62611253), as the SS collector overlaps
    # with the CC collector, since both move objects.
    'art-gtest-ss-gc': {
        'make' :  'test-art-host-gtest',
        'env': {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_READ_BARRIER' : 'false',
            # Disable compact dex to get coverage of standard dex file usage.
            'ART_DEFAULT_COMPACT_DEX_LEVEL' : 'none'
        }
    },
    # TODO: Consider removing this configuration when it is no longer used by
    # any continuous testing target (b/62611253), as the SS collector overlaps
    # with the CC collector, since both move objects.
    'art-gtest-ss-gc-tlab': {
        'make' :  'test-art-host-gtest',
        'env': {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-gtest-debug-gc' : {
        'make' :  'test-art-host-gtest',
        'env' : {
            'ART_TEST_DEBUG_GC' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-generational-cc': {
        'make' : 'test-art-host-gtest',
        'run-test' : [],
        'env' : {
            'ART_USE_GENERATIONAL_CC' : 'true'
        }
    },

    # ASAN (host) configurations.

    # These configurations need detect_leaks=0 to work in non-setup environments like build bots,
    # as our build tools leak. b/37751350

    'art-gtest-asan': {
        'make' : 'test-art-host-gtest',
        'env': {
            'SANITIZE_HOST' : 'address',
            'ASAN_OPTIONS' : 'detect_leaks=0'
        }
    },
    'art-asan': {
        'run-test' : ['--interpreter',
                      '--interp-ac',
                      '--optimizing',
                      '--jit',
                      '--speed-profile'],
        'env': {
            'SANITIZE_HOST' : 'address',
            'ASAN_OPTIONS' : 'detect_leaks=0'
        }
    },
    'art-gtest-heap-poisoning': {
        'make' : 'test-art-host-gtest',
        'env' : {
            'ART_HEAP_POISONING' : 'true',
            'ART_USE_READ_BARRIER' : 'false',
            'SANITIZE_HOST' : 'address',
            'ASAN_OPTIONS' : 'detect_leaks=0'
        }
    },

    # ART Golem build targets used by go/lem (continuous ART benchmarking),
    # (art-opt-cc is used by default since it mimics the default preopt config),
    #
    # calls golem/build-target.sh which builds a golem tarball of the target name,
    #     e.g. 'golem: android-armv7' produces an 'android-armv7.tar.gz' upon success.

    'art-golem-android-armv7': {
        'golem' : 'android-armv7'
    },
    'art-golem-android-armv8': {
        'golem' : 'android-armv8'
    },
    'art-golem-linux-armv7': {
        'golem' : 'linux-armv7'
    },
    'art-golem-linux-armv8': {
        'golem' : 'linux-armv8'
    },
    'art-golem-linux-ia32': {
        'golem' : 'linux-ia32'
    },
    'art-golem-linux-x64': {
        'golem' : 'linux-x64'
    },
    'art-linux-bionic-x64': {
        'build': '{ANDROID_BUILD_TOP}/art/tools/build_linux_bionic_tests.sh {MAKE_OPTIONS}',
        'run-test': ['--run-test-option=--bionic',
                     '--host',
                     '--64',
                     '--no-build-dependencies'],
    },
    'art-linux-bionic-x64-zipapex': {
        'build': '{ANDROID_BUILD_TOP}/art/tools/build_linux_bionic_tests.sh {MAKE_OPTIONS} com.android.art.host',
        'run-test': ['--run-test-option=--bionic',
                     "--runtime-zipapex={SOONG_OUT_DIR}/host/linux_bionic-x86/apex/com.android.art.host.zipapex",
                     '--host',
                     '--64',
                     '--no-build-dependencies'],
    },
}
