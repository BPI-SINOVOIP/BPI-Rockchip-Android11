## read_manifest tool

### 编译方法
- 安装CMake后执行`cmake . && make`

### 使用说明
用于读取manifest，输入的key可以获取对应的value.
```
./read_manifest -i google_pdk.xml --project <project_name> --key remote
```

固定格式为:
```
<project path="xxx" name="xxx" groups="xxx" remote="xxx" revision="xxx" />
```
