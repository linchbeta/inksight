# 配置 API Key

InkSight 支持两种配置方式：

- 在线设备配置（推荐）：在 Web 配置页按设备设置文本/图像模型与 API Key
- 后端环境变量（兜底）：作为默认密钥来源

## 1. 在线设备配置（推荐）

进入设备配置页 ` /config?mac=你的设备MAC `，打开「AI 模型」标签：

- 文本服务商 + 文本模型 + 文本 API Key
- 图像服务商 + 图像模型 + 图像 API Key

保存后会写入该设备配置，后端渲染时优先使用设备配置。

说明：

- API Key 输入框留空表示“不修改已保存值”
- 未配置设备级 Key 时，会回退使用后端环境变量

## 2. 后端环境变量（默认兜底）

```bash
cd inksight/backend
cp .env.example .env
```

编辑 `.env` 填入实际密钥，例如：

- `DEEPSEEK_API_KEY`：文本内容生成默认密钥
- `DASHSCOPE_API_KEY`：图像生成默认密钥（如 ARTWALL）

## 3. 生效方式

环境变量修改后需要重启后端服务：

```bash
python -m uvicorn api.index:app --host 0.0.0.0 --port 8080
```

在线设备配置保存后无需重启，设备下次渲染即按新配置生效。

## 4. 验证建议

- 先用 `STOIC` 预览验证文本配置
- 再用 `ARTWALL` 预览验证图像配置
- 若图像模式无图，优先检查图像 API Key 与图像模型是否可用

## 5. 安全建议

- 不要把 `.env` 提交到仓库
- 生产环境使用平台 Secret 管理
- 定期轮换密钥并限制权限范围
