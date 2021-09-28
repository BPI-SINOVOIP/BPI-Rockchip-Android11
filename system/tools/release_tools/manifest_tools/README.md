## manifest tool

### 编译方法
- 安装CMake后执行`cmake . && make`

### 使用说明
用于整理manifest中的节点顺序, 以固定格式, 便于合并AOSP TAG.
```
./manifest_tool -i google_pdk.xml
```

固定格式为:
```
<project path="xxx" name="xxx" groups="xxx" remote="xxx" revision="xxx" />
```
