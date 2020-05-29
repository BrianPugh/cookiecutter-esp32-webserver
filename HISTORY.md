## v0.3.1 2020-05-28
* Fix `esp_littlfs` submodule within slug via post-gen-hook workaround

## v0.3.0 2020-05-28
* Add SNTP system clock time sync and a basic endpoint `/api/v1/system/time`

## v0.2.1 2020-05-17
* Update `esp_littlefs` to the latest veresion

## v0.2.0 2020-05-17
* Add missing sdkconfig files to slug's .gitignore
* Add acknowledgement to bottom of rendered README
* change sdkconfig.defaults to:
    * Autodetect XTAL frequency
    * Change flash speed to 80MHZ
    * Change flash mode to QIO (fastest)
    * Enable NEWLIB_NANO_FORMAT to save 50KB in the binary. 
* reduced default favicon size from 6429 bytes -> 1621 bytes
* minified html/js/css files to save 1256 bytes

## v0.1.0 2020-05-16
* Initial Release
