#!/usr/bin/python3

# This script is used to update platform SDK prebuilts, Support Library, and a variety of other
# prebuilt libraries used by Android's Makefile builds. For details on how to use this script,
# visit go/update-prebuilts.
import os, sys, getopt, zipfile, re
import argparse
import glob
import subprocess
from shutil import copyfile, rmtree, which, move
from distutils.version import LooseVersion
from functools import reduce

current_path = 'current'
framework_sdk_target = 'sdk_phone_armv7-win_sdk'
support_dir = os.path.join(current_path, 'support')
androidx_dir = os.path.join(current_path, 'androidx')
extras_dir = os.path.join(current_path, 'extras')
buildtools_dir = 'tools'
jetifier_dir = os.path.join(buildtools_dir, 'jetifier', 'jetifier-standalone')

temp_dir = os.path.join(os.getcwd(), "support_tmp")
os.chdir(os.path.dirname(os.path.dirname(os.path.realpath(sys.argv[0]))))
git_dir = os.getcwd()

# See go/fetch_artifact for details on this script.
FETCH_ARTIFACT = '/google/data/ro/projects/android/fetch_artifact'

maven_to_make = {
    # Support Library
    'com.android.support:animated-vector-drawable': {'name':'android-support-animatedvectordrawable', 'path':'graphics/drawable'},
    'com.android.support:appcompat-v7': {'name':'android-support-v7-appcompat', 'path':'v7/appcompat'},
    'com.android.support:cardview-v7': {'name':'android-support-v7-cardview', 'path':'v7/cardview'},
    'com.android.support:collections': {'name':'android-support-collections', 'path':'collections'},
    'com.android.support:customtabs': {'name':'android-support-customtabs', 'path':'customtabs'},
    'com.android.support:exifinterface': {'name':'android-support-exifinterface', 'path':'exifinterface'},
    'com.android.support:gridlayout-v7': {'name':'android-support-v7-gridlayout', 'path':'v7/gridlayout'},
    'com.android.support:leanback-v17': {'name':'android-support-v17-leanback', 'path':'v17/leanback'},
    'com.android.support:mediarouter-v7': {'name':'android-support-v7-mediarouter', 'path':'v7/mediarouter'},
    'com.android.support:palette-v7': {'name':'android-support-v7-palette', 'path':'v7/palette'},
    'com.android.support:percent': {'name':'android-support-percent', 'path':'percent'},
    'com.android.support:preference-leanback-v17': {'name':'android-support-v17-preference-leanback', 'path':'v17/preference-leanback'},
    'com.android.support:preference-v14': {'name':'android-support-v14-preference', 'path':'v14/preference'},
    'com.android.support:preference-v7': {'name':'android-support-v7-preference', 'path':'v7/preference'},
    'com.android.support:recommendation': {'name':'android-support-recommendation', 'path':'recommendation'},
    'com.android.support:recyclerview-v7': {'name':'android-support-v7-recyclerview', 'path':'v7/recyclerview'},
    'com.android.support:support-annotations': {'name':'android-support-annotations', 'path':'annotations'},
    'com.android.support:support-compat': {'name':'android-support-compat', 'path':'compat'},
    'com.android.support:support-core-ui': {'name':'android-support-core-ui', 'path':'core-ui'},
    'com.android.support:support-core-utils': {'name':'android-support-core-utils', 'path':'core-utils'},
    'com.android.support:support-dynamic-animation': {'name':'android-support-dynamic-animation', 'path':'dynamic-animation'},
    'com.android.support:support-emoji-appcompat': {'name':'android-support-emoji-appcompat', 'path':'emoji-appcompat'},
    'com.android.support:support-emoji-bundled': {'name':'android-support-emoji-bundled', 'path':'emoji-bundled'},
    'com.android.support:support-emoji': {'name':'android-support-emoji', 'path':'emoji'},
    'com.android.support:support-fragment': {'name':'android-support-fragment', 'path':'fragment'},
    'com.android.support:support-media-compat': {'name':'android-support-media-compat', 'path':'media-compat'},
    'com.android.support:support-tv-provider': {'name':'android-support-tv-provider', 'path':'tv-provider'},
    'com.android.support:support-v13': {'name':'android-support-v13', 'path':'v13'},
    'com.android.support:support-v4': {'name':'android-support-v4', 'path':'v4'},
    'com.android.support:support-vector-drawable': {'name':'android-support-vectordrawable', 'path':'graphics/drawable'},
    'com.android.support:transition': {'name':'android-support-transition', 'path':'transition'},
    'com.android.support:wear': {'name':'android-support-wear', 'path':'wear'},

    # Support Library (28.0.0 splits + new modules)
    'com.android.support:heifwriter': {'name':'android-support-heifwriter', 'path':'heifwriter'},
    'com.android.support:webkit': {'name':'android-support-webkit', 'path':'webkit'},
    'com.android.support:customview': {'name':'android-support-customview', 'path':'customview'},
    'com.android.support:textclassifier': {'name':'android-support-textclassifier', 'path':'textclassifier'},
    'com.android.support:swiperefreshlayout': {'name':'android-support-swiperefreshlayout', 'path':'swiperefreshlayout'},
    'com.android.support:viewpager': {'name':'android-support-viewpager', 'path':'viewpager'},
    'com.android.support:coordinatorlayout': {'name':'android-support-coordinatorlayout', 'path':'coordinatorlayout'},
    'com.android.support:asynclayoutinflater': {'name':'android-support-asynclayoutinflater', 'path':'asynclayoutinflater'},
    'com.android.support:support-content': {'name':'android-support-support-content', 'path':'support-content'},
    'com.android.support:documentfile': {'name':'android-support-documentfile', 'path':'documentfile'},
    'com.android.support:drawerlayout': {'name':'android-support-drawerlayout', 'path':'drawerlayout'},
    'com.android.support:localbroadcastmanager': {'name':'android-support-localbroadcastmanager', 'path':'localbroadcastmanager'},
    'com.android.support:print': {'name':'android-support-print', 'path':'print'},
    'com.android.support:slidingpanelayout': {'name':'android-support-slidingpanelayout', 'path':'slidingpanelayout'},
    'com.android.support:interpolator': {'name':'android-support-interpolator', 'path':'interpolator'},
    'com.android.support:cursoradapter': {'name':'android-support-cursoradapter', 'path':'cursoradapter'},
    'com.android.support:loader': {'name':'android-support-loader', 'path':'loader'},
    'com.android.support:contentpaging': {'name':'android-support-contentpaging', 'path':'contentpaging'},
    'com.android.support:recyclerview-selection': {'name':'android-support-recyclerview-selection', 'path':'recyclerview-selection'},
    'com.android.support:car': {'name':'android-support-car', 'path':'car'},
    'com.android.support:slices-core': {'name':'android-slices-core', 'path':'slices-core'},
    'com.android.support:slices-view': {'name':'android-slices-view', 'path':'slices-view'},
    'com.android.support:slices-builders': {'name':'android-slices-builders', 'path':'slices-builders'},
    'com.android.support:versionedparcelable': {'name':'android-versionedparcelable', 'path':'versionedparcelable'},

    # Multidex
    'com.android.support:multidex': {'name':'android-support-multidex', 'path':'multidex/library'},
    'com.android.support:multidex-instrumentation': {'name':'android-support-multidex-instrumentation', 'path':'multidex/instrumentation'},

    # Constraint Layout
    'com.android.support.constraint:constraint-layout': {'name':'android-support-constraint-layout', 'path':'constraint-layout'},
    'com.android.support.constraint:constraint-layout-solver': {'name':'android-support-constraint-layout-solver', 'path':'constraint-layout-solver'},

    # Architecture Components
    'android.arch.core:runtime': {'name':'android-arch-core-runtime', 'path':'arch-core/runtime'},
    'android.arch.core:common': {'name':'android-arch-core-common', 'path':'arch-core/common'},
    'android.arch.lifecycle:common': {'name':'android-arch-lifecycle-common', 'path':'arch-lifecycle/common'},
    'android.arch.lifecycle:common-java8': {'name':'android-arch-lifecycle-common-java8', 'path':'arch-lifecycle/common-java8'},
    'android.arch.lifecycle:extensions': {'name':'android-arch-lifecycle-extensions', 'path':'arch-lifecycle/extensions'},
    'android.arch.lifecycle:livedata': {'name':'android-arch-lifecycle-livedata', 'path':'arch-lifecycle/livedata'},
    'android.arch.lifecycle:livedata-core': {'name':'android-arch-lifecycle-livedata-core', 'path':'arch-lifecycle/livedata-core'},
    'android.arch.lifecycle:process': {'name':'android-arch-lifecycle-process', 'path':'arch-lifecycle/process'},
    'android.arch.lifecycle:runtime': {'name':'android-arch-lifecycle-runtime', 'path':'arch-lifecycle/runtime'},
    'android.arch.lifecycle:service': {'name':'android-arch-lifecycle-service', 'path':'arch-lifecycle/service'},
    'android.arch.lifecycle:viewmodel': {'name':'android-arch-lifecycle-viewmodel', 'path':'arch-lifecycle/viewmodel'},
    'android.arch.paging:common': {'name':'android-arch-paging-common', 'path':'arch-paging/common'},
    'android.arch.paging:runtime': {'name':'android-arch-paging-runtime', 'path':'arch-paging/runtime'},
    'android.arch.persistence:db': {'name':'android-arch-persistence-db', 'path':'arch-persistence/db'},
    'android.arch.persistence:db-framework': {'name':'android-arch-persistence-db-framework', 'path':'arch-persistence/db-framework'},
    'android.arch.persistence.room:common': {'name':'android-arch-room-common', 'path':'arch-room/common'},
    'android.arch.persistence.room:migration': {'name':'android-arch-room-migration', 'path':'arch-room/migration'},
    'android.arch.persistence.room:runtime': {'name':'android-arch-room-runtime', 'path':'arch-room/runtime'},
    'android.arch.persistence.room:testing': {'name':'android-arch-room-testing', 'path':'arch-room/testing'},

    # AndroidX
    'androidx.slice:slice-builders': {'name':'androidx.slice_slice-builders', 'path':'androidx/slice/slice-builders'},
    'androidx.slice:slice-core': {'name':'androidx.slice_slice-core', 'path':'androidx/slice/slice-core'},
    'androidx.slice:slice-view': {'name':'androidx.slice_slice-view', 'path':'androidx/slice/slice-view'},
    'androidx.remotecallback:remotecallback': {'name':'androidx.remotecallback_remotecallback', 'path':'androidx/remotecallback/remotecallback'},
    'androidx.remotecallback:remotecallback-processor': {'name':'androidx.remotecallback_remotecallback-processor', 'path':'androidx/remotecallback/remotecallback-processor', 'host':True},
    'androidx.versionedparcelable:versionedparcelable': {'name':'androidx.versionedparcelable_versionedparcelable', 'path':'androidx/versionedparcelable'},
    'androidx.vectordrawable:vectordrawable-animated': {'name':'androidx.vectordrawable_vectordrawable-animated', 'path':'androidx/vectordrawable/vectordrawable-animated'},
    'androidx.activity:activity': {'name':'androidx.activity_activity', 'path':'androidx/activity/activity'},
    'androidx.activity:activity-ktx': {'name':'androidx.activity_activity-ktx', 'path':'androidx/activity/activity-ktx'},
    'androidx.annotation:annotation': {'name':'androidx.annotation_annotation', 'path':'androidx/annotation/annotation', 'host_and_device' : True},
    'androidx.asynclayoutinflater:asynclayoutinflater': {'name':'androidx.asynclayoutinflater_asynclayoutinflater', 'path':'androidx/asynclayoutinflater/asynclayoutinflater'},
    'androidx.car:car': {'name':'androidx.car_car', 'path':'androidx/car/car'},
    'androidx.car:car-cluster': {'name':'androidx.car_car-cluster', 'path':'androidx/car/car-cluster'},
    'androidx.collection:collection': {'name':'androidx.collection_collection', 'path':'androidx/collection/collection'},
    'androidx.collection:collection-ktx': {'name':'androidx.collection_collection-ktx', 'path':'androidx/collection/collection-ktx'},
    'androidx.concurrent:concurrent-futures': {'name':'androidx.concurrent_concurrent-futures', 'path':'androidx/concurrent/concurrent-futures'},
    'androidx.concurrent:concurrent-listenablefuture-callback': {'name':'androidx.concurrent_concurrent-listenablefuture-callback', 'path':'androidx/concurrent/concurrent-listenablefuture-callback'},
    'androidx.concurrent:concurrent-listenablefuture': {'name':'androidx.concurrent_concurrent-listenablefuture', 'path':'androidx/concurrent/concurrent-listenablefuture'},
    'androidx.core:core': {'name':'androidx.core_core', 'path':'androidx/core/core'},
    'androidx.core:core-animation': {'name':'androidx.core_core-animation', 'path':'androidx/core/core-animation'},
    'androidx.core:core-ktx': {'name':'androidx.core_core-ktx', 'path':'androidx/core/core-ktx'},
    'androidx.contentpaging:contentpaging': {'name':'androidx.contentpaging_contentpaging', 'path':'androidx/contentpaging/contentpaging'},
    'androidx.coordinatorlayout:coordinatorlayout': {'name':'androidx.coordinatorlayout_coordinatorlayout', 'path':'androidx/coordinatorlayout/coordinatorlayout'},
    'androidx.legacy:legacy-support-core-ui': {'name':'androidx.legacy_legacy-support-core-ui', 'path':'androidx/legacy/legacy-support-core-ui'},
    'androidx.legacy:legacy-support-core-utils': {'name':'androidx.legacy_legacy-support-core-utils', 'path':'androidx/legacy/legacy-support-core-utils'},
    'androidx.cursoradapter:cursoradapter': {'name':'androidx.cursoradapter_cursoradapter', 'path':'androidx/cursoradapter/cursoradapter'},
    'androidx.browser:browser': {'name':'androidx.browser_browser', 'path':'androidx/browser/browser'},
    'androidx.customview:customview': {'name':'androidx.customview_customview', 'path':'androidx/customview/customview'},
    'androidx.documentfile:documentfile': {'name':'androidx.documentfile_documentfile', 'path':'androidx/documentfile/documentfile'},
    'androidx.drawerlayout:drawerlayout': {'name':'androidx.drawerlayout_drawerlayout', 'path':'androidx/drawerlayout/drawerlayout'},
    'androidx.dynamicanimation:dynamicanimation': {'name':'androidx.dynamicanimation_dynamicanimation', 'path':'androidx/dynamicanimation/dynamicanimation'},
    'androidx.emoji:emoji': {'name':'androidx.emoji_emoji', 'path':'androidx/emoji/emoji'},
    'androidx.emoji:emoji-appcompat': {'name':'androidx.emoji_emoji-appcompat', 'path':'androidx/emoji/emoji-appcompat'},
    'androidx.emoji:emoji-bundled': {'name':'androidx.emoji_emoji-bundled', 'path':'androidx/emoji/emoji-bundled'},
    'androidx.exifinterface:exifinterface': {'name':'androidx.exifinterface_exifinterface', 'path':'androidx/exifinterface/exifinterface'},
    'androidx.fragment:fragment': {'name':'androidx.fragment_fragment', 'path':'androidx/fragment/fragment'},
    'androidx.fragment:fragment-ktx': {'name':'androidx.fragment_fragment-ktx', 'path':'androidx/fragment/fragment-ktx'},
    'androidx.heifwriter:heifwriter': {'name':'androidx.heifwriter_heifwriter', 'path':'androidx/heifwriter/heifwriter'},
    'androidx.interpolator:interpolator': {'name':'androidx.interpolator_interpolator', 'path':'androidx/interpolator/interpolator'},
    'androidx.loader:loader': {'name':'androidx.loader_loader', 'path':'androidx/loader/loader'},
    'androidx.localbroadcastmanager:localbroadcastmanager': {'name':'androidx.localbroadcastmanager_localbroadcastmanager', 'path':'androidx/localbroadcastmanager/localbroadcastmanager'},
    'androidx.media:media': {'name':'androidx.media_media', 'path':'androidx/media/media'},
    'androidx.media2:media2-player': {'name':'androidx.media2_media2-player', 'path':'androidx/media2/media2-player'},
    'androidx.media2:media2-session': {'name':'androidx.media2_media2-session', 'path':'androidx/media2/media2-session'},
    'androidx.media2:media2-common': {'name':'androidx.media2_media2-common', 'path':'androidx/media2/media2-common'},
    'androidx.media2:media2-exoplayer': {'name':'androidx.media2_media2-exoplayer', 'path':'androidx/media2/media2-exoplayer'},
    'androidx.media2:media2-widget': {'name':'androidx.media2_media2-widget', 'path':'androidx/media2/media2-widget'},
    'androidx.navigation:navigation-common': {'name':'androidx.navigation_navigation-common', 'path':'androidx/navigation/navigation-common'},
    'androidx.navigation:navigation-common-ktx': {'name':'androidx.navigation_navigation-common-ktx', 'path':'androidx/navigation/navigation-common-ktx'},
    'androidx.navigation:navigation-fragment': {'name':'androidx.navigation_navigation-fragment', 'path':'androidx/navigation/navigation-fragment'},
    'androidx.navigation:navigation-fragment-ktx': {'name':'androidx.navigation_navigation-fragment-ktx', 'path':'androidx/navigation/navigation-fragment-ktx'},
    'androidx.navigation:navigation-runtime': {'name':'androidx.navigation_navigation-runtime', 'path':'androidx/navigation/navigation-runtime'},
    'androidx.navigation:navigation-runtime-ktx': {'name':'androidx.navigation_navigation-runtime-ktx', 'path':'androidx/navigation/navigation-runtime-ktx'},
    'androidx.navigation:navigation-ui': {'name':'androidx.navigation_navigation-ui', 'path':'androidx/navigation/navigation-ui'},
    'androidx.navigation:navigation-ui-ktx': {'name':'androidx.navigation_navigation-ui-ktx', 'path':'androidx/navigation/navigation-ui-ktx'},
    'androidx.percentlayout:percentlayout': {'name':'androidx.percentlayout_percentlayout', 'path':'androidx/percentlayout/percentlayout'},
    'androidx.print:print': {'name':'androidx.print_print', 'path':'androidx/print/print'},
    'androidx.recommendation:recommendation': {'name':'androidx.recommendation_recommendation', 'path':'androidx/recommendation/recommendation'},
    'androidx.recyclerview:recyclerview-selection': {'name':'androidx.recyclerview_recyclerview-selection', 'path':'androidx/recyclerview/recyclerview-selection'},
    'androidx.savedstate:savedstate': {'name':'androidx.savedstate_savedstate', 'path':'androidx/savedstate/savedstate'},
    'androidx.slidingpanelayout:slidingpanelayout': {'name':'androidx.slidingpanelayout_slidingpanelayout', 'path':'androidx/slidingpanelayout/slidingpanelayout'},
    'androidx.swiperefreshlayout:swiperefreshlayout': {'name':'androidx.swiperefreshlayout_swiperefreshlayout', 'path':'androidx/swiperefreshlayout/swiperefreshlayout'},
    'androidx.textclassifier:textclassifier': {'name':'androidx.textclassifier_textclassifier', 'path':'androidx/textclassifier/textclassifier'},
    'androidx.transition:transition': {'name':'androidx.transition_transition', 'path':'androidx/transition/transition'},
    'androidx.tvprovider:tvprovider': {'name':'androidx.tvprovider_tvprovider', 'path':'androidx/tvprovider/tvprovider'},
    'androidx.legacy:legacy-support-v13': {'name':'androidx.legacy_legacy-support-v13', 'path':'androidx/legacy/legacy-support-v13'},
    'androidx.legacy:legacy-preference-v14': {'name':'androidx.legacy_legacy-preference-v14', 'path':'androidx/legacy/legacy-preference-v14'},
    'androidx.leanback:leanback': {'name':'androidx.leanback_leanback', 'path':'androidx/leanback/leanback'},
    'androidx.leanback:leanback-preference': {'name':'androidx.leanback_leanback-preference', 'path':'androidx/leanback/leanback-preference'},
    'androidx.legacy:legacy-support-v4': {'name':'androidx.legacy_legacy-support-v4', 'path':'androidx/legacy/legacy-support-v4'},
    'androidx.appcompat:appcompat': {'name':'androidx.appcompat_appcompat', 'path':'androidx/appcompat/appcompat'},
    'androidx.appcompat:appcompat-resources': {'name':'androidx.appcompat_appcompat-resources', 'path':'androidx/appcompat/appcompat-resources'},
    'androidx.cardview:cardview': {'name':'androidx.cardview_cardview', 'path':'androidx/cardview/cardview'},
    'androidx.gridlayout:gridlayout': {'name':'androidx.gridlayout_gridlayout', 'path':'androidx/gridlayout/gridlayout'},
    'androidx.mediarouter:mediarouter': {'name':'androidx.mediarouter_mediarouter', 'path':'androidx/mediarouter/mediarouter'},
    'androidx.palette:palette': {'name':'androidx.palette_palette', 'path':'androidx/palette/palette'},
    'androidx.preference:preference': {'name':'androidx.preference_preference', 'path':'androidx/preference/preference'},
    'androidx.recyclerview:recyclerview': {'name':'androidx.recyclerview_recyclerview', 'path':'androidx/recyclerview/recyclerview'},
    'androidx.vectordrawable:vectordrawable': {'name':'androidx.vectordrawable_vectordrawable', 'path':'androidx/vectordrawable/vectordrawable'},
    'androidx.viewpager:viewpager': {'name':'androidx.viewpager_viewpager', 'path':'androidx/viewpager/viewpager'},
    'androidx.viewpager2:viewpager2': {'name':'androidx.viewpager2_viewpager2', 'path':'androidx/viewpager2/viewpager2'},
    'androidx.wear:wear': {'name':'androidx.wear_wear', 'path':'androidx/wear/wear'},
    'androidx.webkit:webkit': {'name':'androidx.webkit_webkit', 'path':'androidx/webkit/webkit'},
    'androidx.biometric:biometric': {'name':'androidx.biometric_biometric', 'path':'androidx/biometric/biometric'},
    'androidx.autofill:autofill': {'name':'androidx.autofill_autofill', 'path':'androidx/autofill/autofill'},

    # AndroidX for Multidex
    'androidx.multidex:multidex': {'name':'androidx-multidex_multidex', 'path':'androidx/multidex/multidex'},
    'androidx.multidex:multidex-instrumentation': {'name':'androidx-multidex_multidex-instrumentation', 'path':'androidx/multidex/multidex-instrumentation'},

    # AndroidX for Constraint Layout
    'androidx.constraintlayout:constraintlayout': {'name':'androidx-constraintlayout_constraintlayout', 'path':'androidx/constraintlayout/constraintlayout'},
    'androidx.constraintlayout:constraintlayout-solver': {'name':'androidx-constraintlayout_constraintlayout-solver', 'path':'androidx/constraintlayout/constraintlayout-solver'},

    # AndroidX for Architecture Components
    'androidx.arch.core:core-common': {'name':'androidx.arch.core_core-common', 'path':'androidx/arch/core/core-common'},
    'androidx.arch.core:core-runtime': {'name':'androidx.arch.core_core-runtime', 'path':'androidx/arch/core/core-runtime'},
    'androidx.lifecycle:lifecycle-common': {'name':'androidx.lifecycle_lifecycle-common', 'path':'androidx/lifecycle/lifecycle-common'},
    'androidx.lifecycle:lifecycle-common-java8': {'name':'androidx.lifecycle_lifecycle-common-java8', 'path':'androidx/lifecycle/lifecycle-common-java8'},
    'androidx.lifecycle:lifecycle-extensions': {'name':'androidx.lifecycle_lifecycle-extensions', 'path':'androidx/lifecycle/lifecycle-extensions'},
    'androidx.lifecycle:lifecycle-livedata': {'name':'androidx.lifecycle_lifecycle-livedata', 'path':'androidx/lifecycle/lifecycle-livedata'},
    'androidx.lifecycle:lifecycle-livedata-ktx': {'name':'androidx.lifecycle_lifecycle-livedata-ktx', 'path':'androidx/lifecycle/lifecycle-livedata-ktx'},
    'androidx.lifecycle:lifecycle-livedata-core': {'name':'androidx.lifecycle_lifecycle-livedata-core', 'path':'androidx/lifecycle/lifecycle-livedata-core'},
    'androidx.lifecycle:lifecycle-livedata-core-ktx': {'name':'androidx.lifecycle_lifecycle-livedata-core-ktx', 'path':'androidx/lifecycle/lifecycle-livedata-core-ktx'},
    'androidx.lifecycle:lifecycle-process': {'name':'androidx.lifecycle_lifecycle-process', 'path':'androidx/lifecycle/lifecycle-process'},
    'androidx.lifecycle:lifecycle-runtime': {'name':'androidx.lifecycle_lifecycle-runtime', 'path':'androidx/lifecycle/lifecycle-runtime'},
    'androidx.lifecycle:lifecycle-runtime-ktx': {'name':'androidx.lifecycle_lifecycle-runtime-ktx', 'path':'androidx/lifecycle/lifecycle-runtime-ktx'},
    'androidx.lifecycle:lifecycle-service': {'name':'androidx.lifecycle_lifecycle-service', 'path':'androidx/lifecycle/lifecycle-service'},
    'androidx.lifecycle:lifecycle-viewmodel': {'name':'androidx.lifecycle_lifecycle-viewmodel', 'path':'androidx/lifecycle/lifecycle-viewmodel'},
    'androidx.lifecycle:lifecycle-viewmodel-ktx': {'name':'androidx.lifecycle_lifecycle-viewmodel-ktx', 'path':'androidx/lifecycle/lifecycle-viewmodel-ktx'},
    'androidx.lifecycle:lifecycle-viewmodel-savedstate': {'name':'androidx.lifecycle_lifecycle-viewmodel-savedstate', 'path':'androidx/lifecycle/lifecycle-viewmodel-savedstate'},
    'androidx.paging:paging-common': {'name':'androidx.paging_paging-common', 'path':'androidx/paging/paging-common'},
    'androidx.paging:paging-common-ktx': {'name':'androidx.paging_paging-common-ktx', 'path':'androidx/paging/paging-common-ktx'},
    'androidx.paging:paging-runtime': {'name':'androidx.paging_paging-runtime', 'path':'androidx/paging/paging-runtime'},
    'androidx.sqlite:sqlite': {'name':'androidx.sqlite_sqlite', 'path':'androidx/sqlite/sqlite'},
    'androidx.sqlite:sqlite-framework': {'name':'androidx.sqlite_sqlite-framework', 'path':'androidx/sqlite/sqlite-framework'},
    'androidx.room:room-common': {'name':'androidx.room_room-common', 'path':'androidx/room/room-common', 'host_and_device' : True},
    'androidx.room:room-compiler': {'name':'androidx.room_room-compiler', 'path':'androidx/room/room-compiler', 'host' : True},
    'androidx.room:room-migration': {'name':'androidx.room_room-migration', 'path':'androidx/room/room-migration', 'host_and_device' : True},
    'androidx.room:room-runtime': {'name':'androidx.room_room-runtime', 'path':'androidx/room/room-runtime'},
    'androidx.room:room-testing': {'name':'androidx.room_room-testing', 'path':'androidx/room/room-testing'},

    # Lifecycle
    # Missing dependencies:
    # - auto-common
    # - javapoet
    #'android.arch.lifecycle:compiler': {'name':'android-arch-lifecycle-compiler', 'path':'arch-lifecycle/compiler'},
    # Missing dependencies:
    # - reactive-streams
    #'android.arch.lifecycle:reactivestreams': 'android-arch-lifecycle-reactivestreams','arch-lifecycle/reactivestreams',

    # Room
    # Missing dependencies:
    # - auto-common
    # - javapoet
    # - antlr4
    # - kotlin-metadata
    # - commons-codec
    #'android.arch.persistence.room:compiler': {'name':'android-arch-room-compiler', 'path':'arch-room/compiler'},
    # Missing dependencies:
    # - rxjava
    #'android.arch.persistence.room:rxjava2': {'name':'android-arch-room-rxjava2', 'path':'arch-room/rxjava2'},

    # Third-party dependencies
    'com.google.android:flexbox': {'name':'flexbox', 'path':'flexbox'},

    # Support Library Material Design Components
    'com.android.support:design': {'name':'android-support-design', 'path':'design'},
    'com.android.support:design-animation': {'name':'android-support-design-animation', 'path':'design-animation'},
    'com.android.support:design-bottomnavigation': {'name':'android-support-design-bottomnavigation', 'path':'design-bottomnavigation'},
    'com.android.support:design-bottomsheet': {'name':'android-support-design-bottomsheet', 'path':'design-bottomsheet'},
    'com.android.support:design-button': {'name':'android-support-design-button', 'path':'design-button'},
    'com.android.support:design-canvas': {'name':'android-support-design-canvas', 'path':'design-canvas'},
    'com.android.support:design-card': {'name':'android-support-design-card', 'path':'design-card'},
    'com.android.support:design-chip': {'name':'android-support-design-chip', 'path':'design-chip'},
    'com.android.support:design-circularreveal': {'name':'android-support-design-circularreveal', 'path':'design-circularreveal'},
    'com.android.support:design-circularreveal-cardview': {'name':'android-support-design-circularreveal-cardview', 'path':'design-circularreveal-cardview'},
    'com.android.support:design-circularreveal-coordinatorlayout': {'name':'android-support-design-circularreveal-coordinatorlayout', 'path':'design-circularreveal-coordinatorlayout'},
    'com.android.support:design-color': {'name':'android-support-design-color', 'path':'design-color'},
    'com.android.support:design-dialog': {'name':'android-support-design-dialog', 'path':'design-dialog'},
    'com.android.support:design-drawable': {'name':'android-support-design-drawable', 'path':'design-drawable'},
    'com.android.support:design-expandable': {'name':'android-support-design-expandable', 'path':'design-expandable'},
    'com.android.support:design-floatingactionbutton': {'name':'android-support-design-floatingactionbutton', 'path':'design-floatingactionbutton'},
    'com.android.support:design-internal': {'name':'android-support-design-internal', 'path':'design-internal'},
    'com.android.support:design-math': {'name':'android-support-design-math', 'path':'design-math'},
    'com.android.support:design-resources': {'name':'android-support-design-resources', 'path':'design-resources'},
    'com.android.support:design-ripple': {'name':'android-support-design-ripple', 'path':'design-ripple'},
    'com.android.support:design-snackbar': {'name':'android-support-design-snackbar', 'path':'design-snackbar'},
    'com.android.support:design-stateful': {'name':'android-support-design-stateful', 'path':'design-stateful'},
    'com.android.support:design-textfield': {'name':'android-support-design-textfield', 'path':'design-textfield'},
    'com.android.support:design-theme': {'name':'android-support-design-theme', 'path':'design-theme'},
    'com.android.support:design-transformation': {'name':'android-support-design-transformation', 'path':'design-transformation'},
    'com.android.support:design-typography': {'name':'android-support-design-typography', 'path':'design-typography'},
    'com.android.support:design-widget': {'name':'android-support-design-widget', 'path':'design-widget'},
    'com.android.support:design-navigation': {'name':'android-support-design-navigation', 'path':'design-navigation'},
    'com.android.support:design-tabs': {'name':'android-support-design-tabs', 'path':'design-tabs'},
    'com.android.support:design-bottomappbar': {'name':'android-support-design-bottomappbar', 'path':'design-bottomappbar'},
    'com.android.support:design-shape': {'name':'android-support-design-shape', 'path':'design-shape'},

    # Androidx Material Design Components
    'com.google.android.material:material': {'name':'com.google.android.material_material', 'path':'com/google/android/material/material'},
}

# Always remove these files.
blacklist_files = [
    'annotations.zip',
    'public.txt',
    'R.txt',
    'AndroidManifest.xml',
    os.path.join('libs','noto-emoji-compat-java.jar')
]

artifact_pattern = re.compile(r"^(.+?)-(\d+\.\d+\.\d+(?:-\w+\d+)?(?:-[\d.]+)*)\.(jar|aar)$")


class MavenLibraryInfo:
    def __init__(self, key, group_id, artifact_id, version, dir, repo_dir, file):
        self.key = key
        self.group_id = group_id
        self.artifact_id = artifact_id
        self.version = version
        self.dir = dir
        self.repo_dir = repo_dir
        self.file = file


def print_e(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def touch(fname, times=None):
    with open(fname, 'a'):
        os.utime(fname, times)


def path(*path_parts):
    return reduce((lambda x, y: os.path.join(x, y)), path_parts)


def flatten(list):
    return reduce((lambda x, y: "%s %s" % (x, y)), list)


def rm(path):
    if os.path.isdir(path):
        rmtree(path)
    elif os.path.exists(path):
        os.remove(path)


def mv(src_path, dst_path):
    if os.path.exists(dst_path):
        rm(dst_path)
    if not os.path.exists(os.path.dirname(dst_path)):
        os.makedirs(os.path.dirname(dst_path))
    for f in (glob.glob(src_path)):
        if '*' in dst_path:
            dst = os.path.join(os.path.dirname(dst_path), os.path.basename(f))
        else:
            dst = dst_path
        move(f, dst)


def detect_artifacts(maven_repo_dirs):
    maven_lib_info = {}

    # Find the latest revision for each artifact, remove others
    for repo_dir in maven_repo_dirs:
        for root, dirs, files in os.walk(repo_dir):
            for file in files:
                if file[-4:] == ".pom":
                    # Read the POM (hack hack hack).
                    group_id = ''
                    artifact_id = ''
                    version = ''
                    file = os.path.join(root, file)
                    with open(file) as pom_file:
                        for line in pom_file:
                            if line[:11] == '  <groupId>':
                                group_id = line[11:-11]
                            elif line[:14] == '  <artifactId>':
                                artifact_id = line[14:-14]
                            elif line[:11] == '  <version>':
                                version = line[11:-11]
                    if group_id == '' or artifact_id == '' or version == '':
                        print_e('Failed to find Maven artifact data in ' + file)
                        continue

                    # Locate the artifact.
                    artifact_file = file[:-4]
                    if os.path.exists(artifact_file + '.jar'):
                        artifact_file = artifact_file + '.jar'
                    elif os.path.exists(artifact_file + '.aar'):
                        artifact_file = artifact_file + '.aar'
                    else:
                        print_e('Failed to find artifact for ' + artifact_file)
                        continue

                    # Make relative to root.
                    artifact_file = artifact_file[len(root) + 1:]

                    # Find the mapping.
                    group_artifact = group_id + ':' + artifact_id
                    if group_artifact in maven_to_make:
                        key = group_artifact
                    elif artifact_id in maven_to_make:
                        key = artifact_id
                    else:
                        print_e('Failed to find artifact mapping for ' + group_artifact)
                        continue

                    # Store the latest version.
                    version = LooseVersion(version)
                    if key not in maven_lib_info \
                            or version > maven_lib_info[key].version:
                        maven_lib_info[key] = MavenLibraryInfo(key, group_id, artifact_id, version,
                                                               root, repo_dir, artifact_file)

    return maven_lib_info


def transform_maven_repos(maven_repo_dirs, transformed_dir, extract_res=True, include_static_deps=True):
    cwd = os.getcwd()

    # Use a temporary working directory.
    maven_lib_info = detect_artifacts(maven_repo_dirs)
    working_dir = temp_dir

    if not maven_lib_info:
        print_e('Failed to detect artifacts')
        return False

    # extract some files (for example, AndroidManifest.xml) from any relevant artifacts
    for info in maven_lib_info.values():
        transform_maven_lib(working_dir, info, extract_res)

    # generate a single Android.bp that specifies to use all of the above artifacts
    makefile = os.path.join(working_dir, 'Android.bp')
    with open(makefile, 'w') as f:
        args = ["pom2bp", "-sdk-version", "current"]
        if include_static_deps:
            args.append("-static-deps")
        rewriteNames = sorted([name for name in maven_to_make if ":" in name] + [name for name in maven_to_make if ":" not in name])
        args.extend(["-rewrite=^" + name + "$=" + maven_to_make[name]['name'] for name in rewriteNames])
        args.extend(["-rewrite=^com.squareup:javapoet$=javapoet-prebuilt-jar"])
        args.extend(["-rewrite=^com.google.guava:listenablefuture$=guava-listenablefuture-prebuilt-jar"])
        args.extend(["-rewrite=^sqlite-jdbc$=xerial-sqlite-jdbc"])
        args.extend(["-rewrite=^gson$=gson-prebuilt-jar"])
        args.extend(["-rewrite=^com.intellij:annotations$=jetbrains-annotations"])
        args.extend(["-extra-static-libs=android-support-car=prebuilt-android.car-stubs"])
        args.extend(["-extra-static-libs=androidx.room_room-compiler=guava-21.0"])
        args.extend(["-host=" + name for name in maven_to_make if maven_to_make[name].get('host')])
        args.extend(["-host-and-device=" + name for name in maven_to_make if maven_to_make[name].get('host_and_device')])
        # these depend on GSON which is not in AOSP
        args.extend(["-exclude=android-arch-room-migration",
                     "-exclude=android-arch-room-testing"])
        args.extend(["."])
        subprocess.check_call(args, stdout=f, cwd=working_dir)

    # Replace the old directory.
    output_dir = os.path.join(cwd, transformed_dir)
    mv(working_dir, output_dir)
    return True

# moves <artifact_info> (of type MavenLibraryInfo) into the appropriate part of <working_dir> , and possibly unpacks any necessary included files
def transform_maven_lib(working_dir, artifact_info, extract_res):
    # Move library into working dir
    new_dir = os.path.normpath(os.path.join(working_dir, os.path.relpath(artifact_info.dir, artifact_info.repo_dir)))
    mv(artifact_info.dir, new_dir)

    matcher = artifact_pattern.match(artifact_info.file)
    maven_lib_name = artifact_info.key
    maven_lib_vers = matcher.group(2)
    maven_lib_type = artifact_info.file[-3:]

    make_lib_name = maven_to_make[artifact_info.key]['name']
    make_dir_name = maven_to_make[artifact_info.key]['path']

    artifact_file = os.path.join(new_dir, artifact_info.file)

    if maven_lib_type == "aar":
        if extract_res:
            target_dir = os.path.join(working_dir, make_dir_name)
            if not os.path.exists(target_dir):
                os.makedirs(target_dir)

            process_aar(artifact_file, target_dir)

        with zipfile.ZipFile(artifact_file) as zip:
            manifests_dir = os.path.join(working_dir, "manifests")
            zip.extract("AndroidManifest.xml", os.path.join(manifests_dir, make_lib_name))

    print(maven_lib_vers, ":", maven_lib_name, "->", make_lib_name)


def process_aar(artifact_file, target_dir):
    # Extract AAR file to target_dir.
    with zipfile.ZipFile(artifact_file) as zip:
        zip.extractall(target_dir)

    # Remove classes.jar
    classes_jar = os.path.join(target_dir, "classes.jar")
    if os.path.exists(classes_jar):
        os.remove(classes_jar)

    # Remove or preserve empty dirs.
    for root, dirs, files in os.walk(target_dir):
        for dir in dirs:
            dir_path = os.path.join(root, dir)
            if not os.listdir(dir_path):
                os.rmdir(dir_path)

    # Remove top-level cruft.
    for file in blacklist_files:
        file_path = os.path.join(target_dir, file)
        if os.path.exists(file_path):
            os.remove(file_path)


def fetch_artifact(target, build_id, artifact_path):
    download_to = os.path.join('.', os.path.dirname(artifact_path))
    print('Fetching %s from %s ...' % (artifact_path, target))
    if not os.path.exists(download_to):
        os.makedirs(download_to)
    fetch_cmd = [FETCH_ARTIFACT, '--bid', str(build_id), '--target', target, artifact_path,
                 download_to]
    print("Running: " + ' '.join(fetch_cmd))
    try:
        subprocess.check_output(fetch_cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError:
        print_e('FAIL: Unable to retrieve %s artifact for build ID %s' % (artifact_path, build_id))
        print_e('Please make sure you are authenticated for build server access!')
        return None
    return artifact_path


def fetch_artifacts(target, build_id, artifact_dict):
    for artifact, target_path in artifact_dict.items():
        artifact_path = fetch_artifact(target, build_id.url_id, artifact)
        if not artifact_path:
            return False
        mv(artifact_path, target_path)
    return True


def extract_artifact(artifact_path):
    # Unzip the repo archive into a separate directory.
    repo_dir = os.path.basename(artifact_path)[:-4]
    with zipfile.ZipFile(artifact_path) as zipFile:
        zipFile.extractall(repo_dir)
    return repo_dir


def fetch_and_extract(target, build_id, file, artifact_path=None):
    if not artifact_path:
        artifact_path = fetch_artifact(target, build_id, file)
    if not artifact_path:
        return None
    return extract_artifact(artifact_path)


def update_support(target, build_id, local_file):
    if build_id:
        repo_file = 'top-of-tree-m2repository-dejetified-%s.zip' % build_id.fs_id
        repo_dir = fetch_and_extract(target, build_id.url_id, repo_file, None)
    else:
        repo_dir = fetch_and_extract(target, None, None, local_file)
    if not repo_dir:
        print_e('Failed to extract Support Library repository')
        return False

    # Transform the repo archive into a Makefile-compatible format.
    return transform_maven_repos([repo_dir], support_dir, extract_res=True)


def update_androidx(target, build_id, local_file):
    if build_id:
        repo_file = 'top-of-tree-m2repository-all-%s.zip' % build_id.fs_id
        repo_dir = fetch_and_extract(target, build_id.url_id, repo_file, None)
    else:
        repo_dir = fetch_and_extract(target, None, None, local_file)
    if not repo_dir:
        print_e('Failed to extract AndroidX repository')
        return False

    # Keep JavaPlugins.bp file untounched.
    java_plugins_bp_path = os.path.join(androidx_dir, 'JavaPlugins.bp')
    tmp_java_plugins_bp_path = os.path.join('/tmp', 'JavaPlugins.bp')
    mv(java_plugins_bp_path, tmp_java_plugins_bp_path)

    # Transform the repo archive into a Makefile-compatible format.
    if not transform_maven_repos([repo_dir], androidx_dir, extract_res=False):
        return False

    # Import JavaPlugins.bp in Android.bp.
    makefile = os.path.join(androidx_dir, 'Android.bp')
    with open(makefile, "a+") as f:
        f.write('\nbuild = ["JavaPlugins.bp"]\n')
    mv(tmp_java_plugins_bp_path, java_plugins_bp_path)

    return True

def update_jetifier(target, build_id):
    repo_file = 'jetifier-standalone.zip'
    repo_dir = fetch_and_extract(target, build_id.url_id, repo_file)
    if not repo_dir:
        print_e('Failed to extract Jetifier')
        return False

    rm(jetifier_dir)
    mv(os.path.join(repo_dir, 'jetifier-standalone'), jetifier_dir)
    os.chmod(os.path.join(jetifier_dir, 'bin', 'jetifier-standalone'), 0o755)
    return True


def update_constraint(target, build_id, local_file):
    if build_id:
        layout_dir = fetch_and_extract(target, build_id.url_id,
                                       'com.android.support.constraint-constraint-layout-%s.zip' % build_id.fs_id)
        solver_dir = fetch_and_extract(target, build_id.url_id,
                                       'com.android.support.constraint-constraint-layout-solver-%s.zip' % build_id.fs_id)
        if not layout_dir or not solver_dir:
            print_e('Failed to extract Constraint Layout repositories')
        return False

        dirs = [layout_dir, solver_dir]
    else:
        repo_dir = extract_artifact(local_file)
        if not repo_dir:
            print_e('Failed to extract Constraint Layout')
            return False
        dirs = [repo_dir]

    # Passing False here is an inelegant solution, but it means we can replace
    # the top-level directory without worrying about other child directories.
    return transform_maven_repos(dirs,
                                os.path.join(extras_dir, 'constraint-layout'), extract_res=False)

def update_constraint_x(local_file):
    repo_dir = extract_artifact(local_file)
    if not repo_dir:
        print_e('Failed to extract Constraint Layout X')
        return False
    return transform_maven_repos([repo_dir], os.path.join(extras_dir, 'constraint-layout-x'), extract_res=False)


def update_design(file):
    design_dir = extract_artifact(file)
    if not design_dir:
        print_e('Failed to extract Design Library repositories')
        return False

    # Don't bother extracting resources -- this should only be used with AAPT2.
    return transform_maven_repos([design_dir],
                                os.path.join(extras_dir, 'material-design'), extract_res=False)


def update_material(file):
    design_dir = extract_artifact(file)
    if not design_dir:
        print_e('Failed to extract intermediate-AndroidX Design Library repositories')
        return False

    # Don't bother extracting resources -- this should only be used with AAPT2.
    return transform_maven_repos([design_dir],
                                 os.path.join(extras_dir, 'material-design-x'), extract_res=False)


def update_framework(build_id, sdk_dir):
    api_scope_list = ['public', 'system', 'test', 'module-lib', 'system-server']
    if sdk_dir == 'current':
        api_scope_list.append('core')

    for api_scope in api_scope_list:
        target_dir = path(sdk_dir, api_scope)
        if api_scope == 'core':
            artifact_to_path = {'core.current.stubs.jar': path(target_dir, 'android.jar')}
        else:
            artifact_to_path = {'apistubs/android/' + api_scope + '/*.jar': path(target_dir, '*')}

        if not fetch_artifacts(framework_sdk_target, build_id, artifact_to_path):
            return False

        if api_scope == 'public':
            # Fetch a few artifacts from the public sdk.
            artifact = 'sdk-repo-linux-platforms-%s.zip' % build_id.fs_id
            artifact_path = fetch_artifact(framework_sdk_target, build_id.url_id, artifact)
            if not artifact_path:
                return False

            with zipfile.ZipFile(artifact_path) as zipFile:
                extra_files = [
                    'android.jar',
                    'framework.aidl',
                    'uiautomator.jar',
                    'core-for-system-modules.jar',
                    'data/annotations.zip',
                    'data/api-versions.xml']
                for filename in extra_files:
                    matches = list(filter(lambda path: filename in path, zipFile.namelist()))
                    if len(matches) != 1:
                        print_e('Expected 1 file named \'%s\' in zip %s, found %d' %
                                (filename, zipFile.filename, len(matches)))
                        return False
                    zip_path = matches[0]
                    src_path = zipFile.extract(zip_path)
                    dst_path = path(target_dir, filename)
                    mv(src_path, dst_path)

    return True

def update_makefile(build_id):
    template = '"%s",\n\
        "current"'
    makefile = os.path.join(git_dir, 'Android.bp')

    with open(makefile, 'r+') as f:
        contents = f.read().replace('"current"', template % build_id)
        f.seek(0)
        f.write(contents)

    return True

def finalize_sdk(build_id, sdk_version):
    target_finalize_dir = '%d' % sdk_version

    for api_scope in ['public', 'system', 'test', 'module-lib', 'system-server']:
        artifact_to_path = {'apistubs/android/' + api_scope + '/api/*.txt':
                            path(target_finalize_dir, api_scope, 'api', '*')}

        if not fetch_artifacts(framework_sdk_target, build_id, artifact_to_path):
            return False

    return update_framework(build_id, target_finalize_dir) \
            and update_makefile(target_finalize_dir)


def update_framework_current(build_id):
    return update_framework(build_id, current_path)


def update_buildtools(target, arch, build_id):
    artifact_path = fetch_and_extract(target, build_id.url_id,
                                   "sdk-repo-%s-build-tools-%s.zip" % (arch, build_id.fs_id))
    if not artifact_path:
        return False

    top_level_dir = os.listdir(artifact_path)[0]
    src_path = os.path.join(artifact_path, top_level_dir)
    dst_path = path(buildtools_dir, arch)
    mv(src_path, dst_path)

    # Move all top-level files to /bin and make them executable
    bin_path = path(dst_path, 'bin')
    top_level_files = filter(lambda e: os.path.isfile(path(dst_path, e)), os.listdir(dst_path))
    for file in top_level_files:
        src_file = path(dst_path, file)
        dst_file = path(bin_path, file)
        mv(src_file, dst_file)
        os.chmod(dst_file, 0o755)

    # Remove renderscript
    rm(path(dst_path, 'renderscript'))

    return True


def append(text, more_text):
    if text:
        return "%s, %s" % (text, more_text)
    return more_text


class buildId(object):
  def __init__(self, url_id, fs_id):
    # id when used in build server urls
    self.url_id = url_id
    # id when used in build commands
    self.fs_id = fs_id

def getBuildId(args):
  # must be in the format 12345 or P12345
  source = args.source
  number_text = source[:]
  presubmit = False
  if number_text.startswith("P"):
    presubmit = True
    number_text = number_text[1:]
  if not number_text.isnumeric():
    return None
  url_id = source
  fs_id = url_id
  if presubmit:
    fs_id = "0"
  args.file = False
  return buildId(url_id, fs_id)

def getFile(args):
  source = args.source
  if not source.isnumeric():
    return args.source
  return None


def script_relative(rel_path):
    return os.path.join(script_dir, rel_path)


def uncommittedChangesExist():
    try:
        # Make sure we don't overwrite any pending changes.
        diffCommand = "cd " + git_dir + " && git diff --quiet"
        subprocess.check_call(diffCommand, shell=True)
        subprocess.check_call(diffCommand + " --cached", shell=True)
        return False
    except subprocess.CalledProcessError:
        return True


rm(temp_dir)
parser = argparse.ArgumentParser(
    description=('Update current prebuilts'))
parser.add_argument(
    'source',
    help='Build server build ID or local Maven ZIP file')
parser.add_argument(
    '-d', '--design', action="store_true",
    help='If specified, updates only the Design Library')
parser.add_argument(
    '-m', '--material', action="store_true",
    help='If specified, updates only the intermediate-AndroidX Design Library')
parser.add_argument(
    '-c', '--constraint', action="store_true",
    help='If specified, updates only Constraint Layout')
parser.add_argument(
    '--constraint_x', action="store_true",
    help='If specified, updates Constraint Layout X')
parser.add_argument(
    '-j', '--jetifier', action="store_true",
    help='If specified, updates only Jetifier')
parser.add_argument(
    '-p', '--platform', action="store_true",
    help='If specified, updates only the Android Platform')
parser.add_argument(
    '-f', '--finalize_sdk', type=int,
    help='If specified, imports the source build as the specified finalized SDK version')
parser.add_argument(
    '-b', '--buildtools', action="store_true",
    help='If specified, updates only the Build Tools')
parser.add_argument(
    '--stx', action="store_true",
    help='If specified, updates Support Library and Androidx (that is, all artifacts built from frameworks/support)')
parser.add_argument(
    '--commit-first', action="store_true",
    help='If specified, then if uncommited changes exist, commit before continuing')
args = parser.parse_args()
if args.stx:
    args.androidx = True
else:
    args.androidx = False
args.support = False
args.file = True
if not args.source:
    parser.error("You must specify a build ID or local Maven ZIP file")
    sys.exit(1)
if not (args.support or args.platform or args.constraint or args.buildtools \
                or args.design or args.jetifier or args.androidx or args.material \
                or args.finalize_sdk or args.constraint_x):
    parser.error("You must specify at least one target to update")
    sys.exit(1)
if (args.support or args.constraint or args.constraint_x or args.design or args.material or args.androidx) \
        and which('pom2bp') is None:
    parser.error("Cannot find pom2bp in path; please run lunch to set up build environment. You may also need to run 'm pom2bp' if it hasn't been built already.")
    sys.exit(1)

if uncommittedChangesExist():
    if args.commit_first:
        subprocess.check_call("cd " + git_dir + " && git add -u", shell=True)
        subprocess.check_call("cd " + git_dir + " && git commit -m 'save working state'", shell=True)

if uncommittedChangesExist():
    print_e('FAIL: There are uncommitted changes here. Please commit or stash before continuing, because %s will run "git reset --hard" if execution fails' % os.path.basename(__file__))
    sys.exit(1)

try:
    components = None
    if args.constraint:
        if update_constraint('studio', getBuildId(args), getFile(args)):
            components = append(components, 'Constraint Layout')
        else:
            print_e('Failed to update Constraint Layout, aborting...')
            sys.exit(1)
    if args.constraint_x:
        if update_constraint_x(getFile(args)):
            components = append(components, 'Constraint Layout X')
        else:
            print_e('Failed to update Constraint Layout X, aborting...')
            sys.exit(1)
    if args.support:
        if update_support('androidx', getBuildId(args), getFile(args)):
            components = append(components, 'Support Library')
        else:
            print_e('Failed to update Support Library, aborting...')
            sys.exit(1)
    if args.androidx:
        if update_androidx('androidx', \
                           getBuildId(args), getFile(args)):
            components = append(components, 'AndroidX')
        else:
            print_e('Failed to update AndroidX, aborting...')
            sys.exit(1)
    if args.jetifier:
        if update_jetifier('androidx', getBuildId(args)):
            components = append(components, 'Jetifier')
        else:
            print_e('Failed to update Jetifier, aborting...')
            sys.exit(1)
    if args.platform or args.finalize_sdk:
        if update_framework_current(getBuildId(args)):
            components = append(components, 'platform SDK')
        else:
            print_e('Failed to update platform SDK, aborting...')
            sys.exit(1)
    if args.finalize_sdk:
        n = args.finalize_sdk
        if finalize_sdk(getBuildId(args), n):
            # We commit the finalized dir separately from the current sdk update.
            msg = "Import final sdk version %d from build %s" % (n, getBuildId(args).url_id)
            subprocess.check_call(['git', 'add', '%d' % n])
            subprocess.check_call(['git', 'add', 'Android.bp'])
            subprocess.check_call(['git', 'commit', '-m', msg])
        else:
            print_e('Failed to finalize SDK %d, aborting...' % n)
            sys.exit(1)
    if args.design:
        if update_design(getFile(args)):
            components = append(components, 'Design Library')
        else:
            print_e('Failed to update Design Library, aborting...')
            sys.exit(1)
    if args.material:
        if update_material(getFile(args)):
            components = append(components, 'intermediate-AndroidX Design Library')
        else:
            print_e('Failed to update intermediate-AndroidX Design Library, aborting...')
            sys.exit(1)
    if args.buildtools:
        if update_buildtools('sdk_phone_armv7-sdk_mac', 'darwin', getBuildId(args)) \
                and update_buildtools('sdk_phone_x86_64-sdk', 'linux', getBuildId(args)) \
                and update_buildtools('sdk_phone_armv7-win_sdk', 'windows', getBuildId(args)):
            components = append(components, 'build tools')
        else:
            print_e('Failed to update build tools, aborting...')
            sys.exit(1)



    subprocess.check_call(['git', 'add', current_path, buildtools_dir])
    if not args.source.isnumeric():
        src_msg = "local Maven ZIP"
    else:
        src_msg = "build %s" % (getBuildId(args).url_id)
    msg = "Import %s from %s\n\n%s" % (components, src_msg, flatten(sys.argv))
    subprocess.check_call(['git', 'commit', '-m', msg])
    if args.finalize_sdk:
        print('NOTE: Created two commits:')
        subprocess.check_call(['git', 'log', '-2', '--oneline'])
    print('Remember to test this change before uploading it to Gerrit!')

finally:
    # Revert all stray files, including the downloaded zip.
    try:
        with open(os.devnull, 'w') as bitbucket:
            subprocess.check_call(['git', 'add', '-Af', '.'], stdout=bitbucket)
            subprocess.check_call(
                ['git', 'commit', '-m', 'COMMIT TO REVERT - RESET ME!!!', '--allow-empty'], stdout=bitbucket)
            subprocess.check_call(['git', 'reset', '--hard', 'HEAD~1'], stdout=bitbucket)
    except subprocess.CalledProcessError:
        print_e('ERROR: Failed cleaning up, manual cleanup required!!!')
