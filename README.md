# Linux Char Driver with Dynamic Buffer  

Драйвер с динамическим буфером (`kmalloc`), управлением через `ioctl` и `sysfs`.  

## Функционал:  
- Стандартные файловые операции: `read`, `write`, `open`  
- `ioctl`: очистка буфера, запрос/изменение размера  
- `sysfs`: управление размером буфера  
- Мьютексы для синхронизации  

## Сборка и загрузка:  
```sh
make  
sudo insmod hello_cdev.ko
```

## Использование:  
```sh
echo "test" > /dev/hello_cdev  
cat /dev/hello_cdev  
```

## Управление:  
```sh
sudo ioctl /dev/hello_cdev CLEAR_BUFFER  
echo 128 > /sys/kernel/hello_cdev/buffer_size  
```
