## hplip

#### 简介

hplip是一款通过惠普打印机打印，扫描，传真的工具软件，HPLIP(全称HP Linux Imaging and Printing)是一个由惠普公司发起和领导的项目，其目标是让Linux系统，能够完成支持惠普的喷墨打印机和激光打印机的打印，扫描和传真功能。它所提供的驱动程序，共支持约2000部HP打印机型号。

本项目基于最新的3.16.3版本。

-------------

#### 编译前的准备

编译前所有需要对源代码修改的准备工作已经在项目中完成。

######1.工具链使用NDK导出的交叉编译工具

我使用的是`android-ndk-r11`导出的工具链。

###### 2.configure会找不到-lpthread

修改configure文件中的

```shell
LIBS="-lpthread  $LIBS" ====>  LIBS="-lc  $LIBS"
```

###### 3.configure找不到-lusb

为了方便使用usb库，可以从我的仓库中找到[地址](https://github.com/jianglei12138/usb)。把对应的头文件和动态库放置到交叉编译工具链中的sysroot文件夹下。

修改configure文件中的

```shell
LIBS="-lusb-1.0  $LIBS" ====> LIBS="-lusb  $LIBS"
```

###### 4.configure找不到python

从我的python仓库中获取头文件python3的头文件放置到交叉编译工具链中的sysroot文件夹下的usr/include/python3.5m。

修改configure文件中的

```shell
am_cv_python_version=`$PYTHON -c "import sys; sys.stdout.write(sys.version[:3])"`
#改为指定的版本
am_cv_python_version="3.5"
```

以及查找python头文件的部分代码

```shell
   for ac_header in python${PYTHON_VERSION}/Python.h python${PYTHON_VERSION}mu/Python.h python${PYTHON_VERSION}m/Python.h
do :
  as_ac_Header=`$as_echo "ac_cv_header_$ac_header" | $as_tr_sh`
ac_fn_c_check_header_mongrel "$LINENO" "$ac_header" "$as_ac_Header" "$ac_includes_default"
if eval test \"x\$"$as_ac_Header"\" = x"yes"; then :
  cat >>confdefs.h <<_ACEOF
#define `$as_echo "HAVE_$ac_header" | $as_tr_cpp` 1
_ACEOF
 FOUND_HEADER=yes; break;
fi

done
#改成确定的版本
  for ac_header in python${PYTHON_VERSION}m/Python.h
do :
  cat >>confdefs.h <<_ACEOF
#define `$as_echo "HAVE_$ac_header" | $as_tr_cpp` 1
_ACEOF
 FOUND_HEADER=yes;
done
```

######5.configure找不到dbus

为了使用dbus库，可以从我的仓库中找到[地址](https://github.com/jianglei12138/dbus)。把对应的头文件和动态库放置到交叉编译工具链中的sysroot文件夹下。

修改configure文件去除dbus的检测

```shell
if test -n "$DBUS_LIBS"; then
    pkg_cv_DBUS_LIBS="$DBUS_LIBS"
 elif test -n "$PKG_CONFIG"; then
    if test -n "$PKG_CONFIG" && \
    { { $as_echo "$as_me:${as_lineno-$LINENO}: \$PKG_CONFIG --exists --print-errors \"dbus-1 >= 1.0.0\""; } >&5
  ($PKG_CONFIG --exists --print-errors "dbus-1 >= 1.0.0") 2>&5
  ac_status=$?
  $as_echo "$as_me:${as_lineno-$LINENO}: \$? = $ac_status" >&5
  test $ac_status = 0; }; then
  pkg_cv_DBUS_LIBS=`$PKG_CONFIG --libs "dbus-1 >= 1.0.0" 2>/dev/null`
		      test "x$?" != "x0" && pkg_failed=yes
else
  pkg_failed=yes
fi
 else
    pkg_failed=untried
fi
#后添加
pkg_failed=no
```

###### 6.编译时utils冲突类型__errno

注释掉common/utils.c中的

```c
 //extern int errno;
```

###### 7.编译时找不到-lpthread与-lusb-1.0

修改Makefile.in

```shell
@HPLIP_BUILD_TRUE@@LIBUSB01BUILD_FALSE@amappend9 = -lusb-1.0
@HPLIP_BUILD_TRUE@libhpmud_la_LDFLAGS = -version-info 0:6:0 -lpthread \
#改为
@HPLIP_BUILD_TRUE@@LIBUSB01BUILD_FALSE@amappend9 = -lusb
@HPLIP_BUILD_TRUE@libhpmud_la_LDFLAGS = -version-info 0:6:0 -lc \
```

###### 8.编译时未定义dbus类函数

修改Makefile.in

```shell
DBUS_LIBS = @DBUS_LIBS@
#改为
DBUS_LIBS = -ldbus
```

###### 9.编译pthread_cancel未定义

bionic 不支持pthread_cancel()。修改prnt/backend/hp.c与io/hpmud/musb.c，修改为

```c
int status;
if ( (status = pthread_kill(pthread_id, SIGUSR1)) != 0)   
{   
    printf("Error cancelling thread %d", pthread_id);  
}   
```

在create时添加对SIGUSR1信号的处理，即在当前文件的pjl_read_thread或者write_thread函数中添加

```c
signal(SIGUSR1,handle_quit);
```

以及在之前添加函数

```c
void handle_quit(int sign)
{
    pthread_exit(NULL);
}
```

###### 10.编译getdomainname未定义

修改prnt/hpcups/HPCupsFilter.cpp添加getdomainname函数

```c
int getdomainname (char *name,size_t len)
{
  struct utsname u;
  size_t u_len;
  if (uname (&u) < 0)
    return -1;
  u_len = strlen (u.domainname);
  memcpy (name, u.domainname, MIN (u_len + 1, len));
  return 0;
}
```

-------------

#### 编译

###### 1.configure

```shell
 ./configure --disable-network-build --host=arm-linux-androideabi --disable-scan-build --disable-qt4  --disable-gui-build  --with-cupsbackenddir=/system/usr/root/lib/cups/backend  --with-cupsfilterdir=/system/usr/root/lib/cups/filter  --with-drvdir=/system/usr/root/share/cups/drv  --with-mimedir=/system/usr/root/share/cups/mime --prefix=/system/usr/ --sysconfdir=/system/etc  --localstatedir=/system/usr/var 
```

###### 2.make

```shell
make
```

###### 3.install

```shell
make install DESTDIR=/path/you/like
```

--------------

#### 注意

由于hplip严重依赖python，因此必须安装python for android。并且我并没有完整的测试，因为我没有hp打印机，更严格的说我一台打印机都没有。因此我只提供移植的大致流程~~~



