# CI/CD 自动构建说明

本项目使用 GitHub Actions 实现自动化构建和发布。

## 自动构建触发条件

1. **Push 到 main 分支** - 自动构建所有平台版本
2. **创建 Pull Request** - 自动构建并测试
3. **创建版本标签 (v*)** - 自动构建并发布 Release

## 构建平台

- ✅ Windows (win.xpl)
- ✅ Linux (lin.xpl)
- ✅ macOS (mac.xpl)

## 如何发布新版本

### 1. 创建版本标签
```bash
git tag v1.0.0
git push origin v1.0.0
```

### 2. 自动流程
- GitHub Actions 自动在三个平台上编译
- 生成包含所有平台的压缩包
- 自动创建 GitHub Release
- 上传 `ToLissFCUMonitor-v1.0.0.zip`

### 3. 用户下载
用户可以从 Releases 页面下载，压缩包内包含：
```
ToLissFCUMonitor/
  ├── win.xpl      (Windows)
  ├── lin.xpl      (Linux)
  └── mac.xpl      (macOS)
```

## X-Plane SDK

- SDK 自动从官方下载：`XPSDK301.zip`
- 使用缓存机制，加速后续构建
- 无需手动下载或提交 SDK 到仓库

## 本地构建

如果需要本地构建并自动复制到 X-Plane：

```bash
cmake -B build -DXPLANE_PATH="E:/software/X-Plane 12"
cmake --build build --config Release
```

如果不设置 `XPLANE_PATH`，插件只会输出到 `dist/` 目录。
