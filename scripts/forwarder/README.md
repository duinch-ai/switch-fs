# Switch FS Forwarder NSP (Biểu tượng Màn hình chính Home Menu)

Tạo file `SwitchFS.nsp` cài đặt lên Switch để chạy Switch FS ngoài màn hình chính (Title mode với Full RAM, không bị giới hạn Applet mode):

```shell
cmake -B build-forwarder scripts/forwarder
cmake --build build-forwarder --target forwarder.nsp
```

Hoặc cài trực tiếp bằng hacbrewpack:
```shell
hacbrewpack -k prod.keys --titleid 010FF000FFFF0005 --titlename "Switch FS" --noromfs --nologo
```
