set_target_properties(LLVMPolly PROPERTIES
        IMPORTED_LOCATION_RELEASE ${CMAKE_CURRENT_LIST_DIR}/../../../lib/LLVMPolly.so)
set_target_properties(PollyISL PROPERTIES
        IMPORTED_LOCATION_RELEASE ${CMAKE_CURRENT_LIST_DIR}/../../../lib/libPollyISL.a)
set_target_properties(Polly PROPERTIES
        IMPORTED_LOCATION_RELEASE ${CMAKE_CURRENT_LIST_DIR}/../../../lib/libPolly.a)
