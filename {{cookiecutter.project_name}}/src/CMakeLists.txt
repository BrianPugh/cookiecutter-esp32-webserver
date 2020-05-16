
idf_component_register(
        SRCS
            "main.c"
            "server.c"
            "filesystem.c"
            "led.c"
            "route.c"
            "route/v1/example.c"
            "route/v1/filesystem.c"
            "route/v1/nvs.c"
            "route/v1/ota.c"
            "route/v1/system.c"
        INCLUDE_DIRS
            "."
        REQUIRES
            "app_update"
            "esp_http_client"
            "esp_http_server"
            "esp_https_ota"
            "esp_littlefs"
            "esp_system"
            "esp_wifi"
            "fatfs"
            "json"
            "mdns"
            "nvs_flash"
            "libsodium"
        EMBED_FILES
            # The names are generated from the full name of the file, as given in EMBED_FILES.
            # Characters /, ., etc. are replaced with underscores.
            # NOTE: The path to this file doesn't get converted as part of the name.
            #    i.e. `foo/bar.baz` will get turned into:
            #            _binary_bar_baz_start and _binary_bar_baz_end
            # The _binary prefix in the symbol name is added by objcopy and is the same for both text and binary files.
            # e.g:
            #        extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
            #        extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");
            "html/api_v1_filesystem.html"
            "html/api_v1_nvs.js"
            "html/common.css"
            "html/favicon.ico"
)
