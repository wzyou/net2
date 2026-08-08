function(netp2_enable_sanitizers target_name)
    if(MSVC)
        return()
    endif()

    if(NETP2_ENABLE_ASAN AND NETP2_ENABLE_TSAN)
        message(FATAL_ERROR "ASan and TSan cannot be enabled together")
    endif()

    if(NETP2_ENABLE_ASAN)
        target_compile_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address)
    endif()

    if(NETP2_ENABLE_TSAN)
        target_compile_options(${target_name} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=thread)
    endif()
endfunction()
