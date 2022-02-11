## release tool

### 编译方法
- 支持CMake, 安装CMake后执行`cmake . && make`
- 支持Android Make, 源码放到system/tools/release_tools后执行`mm -j4`

### 使用说明
build脚本生成的manifest通常无法直接用来回朔或代码定点发布等, 并且存在 需要切分支/remote修改或部分工程需要隐藏 等问题;
本工具用于manifest的更新, 适用于代码定点发布, 固件版本回朔等, 使用方法不固定, 可以自行发掘.

- 映射服务器mainfest定点更新, 如10.10.10.78服务器作为29服务器的映射, 后期持续维护中只会更新AOSP的代码(只更新tag)以及部分rk稳定的仓库(用hash值固定), 以下命令可以帮助更新78服务器中的那些稳定仓库的hash值.
```
release_tool -i rk3326_repository.xml -b manifest_20181119.1436.xml -o rk3326_repository_new.xml [-dir ./PATCHES, 如manifest_2018xxx.xml存在本地提交, 使用这个参数恢复]
```

- 对外服务器定点更新, 对外推送时希望更新对外的manifest中的hash值, 以往都是对比build脚本生成的manifest手动修改hash值, 用脚本机械化会更简单省时.
```
release_tool -i rk3326_pie_sdk_express_release_v1.0.xml -b manifest_20181119.1436.xml -o rk3326_repository_new.xml [-dir ./PATCHES, 如manifest_2018xxx.xml存在本地提交, 根据提示的分支处理]
```

- 代码回朔, 由于build脚本生成的manifest只是所有project最新点的快照, 无法区分本地提交或是服务器提交, 用以下命令可以恢复这些有问题的hash值, 输出的manifest能够直接sync, 配合restore_patches.sh能够恢复本地的提交和补丁
```
release_tool -i manifest_20181119.1436.xml -b manifest_20181119.1436.xml -o manifest_temp.xml -dir ./PATCHES
```

- 更新时保持某些仓库的点不更新, 注意: 配置文件的优先级比补丁包[-dir参数]高!
```
release_tool -i manifest_20181119.1436.xml -b manifest_20181119.1436.xml -o manifest_temp.xml -dir ./PATCHES -c config.ini
```

### 参数说明
```
-i      需要基于这些工程进行更新的manifest文件, 需要更新这个列表内的hash值;
-b      使用 `build.sh` 或 `repo manifest` 命令编译出的manifest文件, 包含所有SDK工程的hash值;
-o      输出文件, 只更新-i参数中文件的hash值, 不会修改仓库列表及remote地址等;
-c      使用配置文件, 可以用来过滤project, 使用时此参数时, 当遍历到属于[Filter]这个session下的project时, 会保留原始commitID, 该project的hash值不会更新, 配置文件可以参考config.ini;
-dir    用于恢复服务器不存在的hash值, 用于代码回朔, 遍历 build.sh脚本所生成的 git-merge-base.txt 文件, 输出-o文件后可直接用于sync.
```

### 一键切换到补丁状态
- SDK根目录执行以下脚本, 能够自动切换至补丁包中git-merge-base.txt的点并打好其中的补丁:
```
./restore_patches.sh [可选, 补丁包路径, 若不指定则默认为: SDK/PATCHES]
```

### AOSP + Gerrit工具
这两个脚本提供了新工程整合时的操作，注意，脚本依赖gitolite_tool，可以用cmake自行编译下
- 检查xml中的project是否都在gerrit上创建过，如果没有创建，则创建，注意要删除kernel等无用的工程，以免浪费gerrit的空间
```shell
./create_gerrit_projects.sh /path/to/xml
```
- 检查上一步中去掉的gerrit上无用的工程，生成remove.xml，整合工程时include即可
```shell
./genernate_remove.sh /path/to/xml output.xml
```
