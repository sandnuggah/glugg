idf_component_register(SRCS "pairing_code.c" "slot_table.c" "backoff.c" "sensor_mgr.c" "command.c" "thread_channel.c"
                       INCLUDE_DIRS "include"
                       REQUIRES led_pattern)
