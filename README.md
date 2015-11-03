# BlkDeviceDriver
自前のmake_request関数を使用するブロック型デバイスドライバの例

    make
    insmod hello.ko
    dd if=/dev/urandom of=/dev/bhelloa bs=512 count=1 oflag=direct
