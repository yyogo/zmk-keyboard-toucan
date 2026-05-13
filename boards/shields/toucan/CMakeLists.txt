zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../../include)

if((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources(behavior_toucan_caps_word.c)
endif()

if(CONFIG_TOUCAN_HEARTBEAT)
  zephyr_library_sources(heartbeat.c)
endif()
