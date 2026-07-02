# PROJ_ClaudePulse

Claude Code が現在使用しているモデル名とトークン使用量を、心電図風のパルスメータでリアルタイム表示する Windows デスクトップツール（C++ / MFC）です。

## ClaudePulse のビルド・実行（Dev Container 外の Windows で行う）

1. Windows 上で `ClaudePulse.sln` を Visual Studio 2022 で開く（「C++ による MFC デスクトップ開発」ワークロードが必要）
2. 構成 `Debug | x64` または `Release | x64` を選択してビルド
3. `ClaudePulse.exe` を起動すると、`%USERPROFILE%\.claude\projects` 配下の最新セッション記録（*.jsonl）を1秒間隔で監視し、モデル名・トークン使用量・消費レートを表示します

仕様・設計は [.steering/](.steering/)、タスクは [tasks/tasklist.md](tasks/tasklist.md) を参照してください。

---

# Node.js + Claude Code Dev Container

Node.js と Claude Code がプリインストールされた開発コンテナのベーステンプレートです。

## 必要なもの

- [Docker Desktop](https://www.docker.com/products/docker-desktop/)
- [Visual Studio Code](https://code.visualstudio.com/)
- VS Code拡張機能: [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)

## セットアップ

### 1. コンテナを起動

1. VS Code でこのフォルダを開く
2. 右下に表示される **「Reopen in Container」** をクリック
   （または `Ctrl+Shift+P` → `Dev Containers: Reopen in Container`）
3. コンテナのビルドが完了するまで待つ（初回は数分かかります）

### 2. 動作確認

ターミナルで確認：
```bash
node --version   # Node.js のバージョン
claude --version # Claude Code のバージョン
```

## 含まれるもの

| ツール | バージョン |
|--------|-----------|
| Node.js | 20.x (LTS) |
| Claude Code | 最新版 |

## 使い方

コンテナ内のターミナルで Claude Code を起動：
```bash
claude
```

## 注意事項

- `.claude` フォルダもローカルとコンテナ間で共有されます（認証情報・設定の共有）
