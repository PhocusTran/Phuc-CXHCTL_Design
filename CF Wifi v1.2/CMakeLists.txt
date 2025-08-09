idf_component_register( SRCS "wifi_server.c"
                             "file_server.c"
                             "mount.c"
                             "usb_msc.c"
                             "lock.c"
                             "main.c" 
                        INCLUDE_DIRS "."
                        EMBED_FILES ./static/output.css ./static/script.js)
