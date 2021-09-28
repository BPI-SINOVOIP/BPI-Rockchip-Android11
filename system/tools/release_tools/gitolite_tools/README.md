## gitolite tool

### 编译方法
- 支持CMake, 安装CMake后执行`cmake . && make`

### 使用说明
用于提取其中的所有repo name, 添加prefix和endfix后能够更新gitolite的仓库列表.
Android大版本更新后, 很多仓库会变化, 用此工具提取manifest中的所有仓库列表, 更简单省时省力.

```
./gitolite_tool -i google_pdk.xml
```
